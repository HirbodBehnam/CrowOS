#include "rtc.h"
#include "common/printf.h"
#include "cpu/asm.h"

/**
 * From
 * https://github.com/torvalds/linux/blob/c4b9570cfb63501638db720f3bee9f6dfd044b82/arch/x86/kernel/tsc.c
 * and
 * https://github.com/dreamos82/Dreamos64/blob/76a6015e69ca3b2aae022ed2f0ee1258444a9416/src/kernel/hardware/rtc.c
 */

#define PIT_TICK_RATE 1193182ul

/*
 * How many MSB values do we want to see? We aim for
 * a maximum error rate of 500ppm (in practice the
 * real error is much smaller), but refuse to spend
 * more than 50ms on it.
 */
#define MAX_QUICK_PIT_MS 50
#define MAX_QUICK_PIT_ITERATIONS (MAX_QUICK_PIT_MS * PIT_TICK_RATE / 1000 / 256)

static inline int pit_verify_msb(unsigned char val) {
  /* Ignore LSB */
  inb(0x42);
  return inb(0x42) == val;
}

static inline int pit_expect_msb(unsigned char val, uint64_t *tscp,
                                 uint64_t *deltap) {
  int count;
  uint64_t tsc = 0, prev_tsc = 0;

  for (count = 0; count < 50000; count++) {
    if (!pit_verify_msb(val))
      break;
    prev_tsc = tsc;
    tsc = get_tsc();
  }
  *deltap = get_tsc() - prev_tsc;
  *tscp = tsc;

  /*
   * We require _some_ success, but the quality control
   * will be based on the error terms on the TSC values.
   */
  return count > 5;
}

/**
 * Finds the frequency of TSC increments by using the PIT.
 */
static uint64_t quick_pit_calibrate(void) {
  int i;
  uint64_t tsc, delta, d1, d2;

  /* Set the Gate high, disable speaker */
  outb(0x61, (inb(0x61) & ~0x02) | 0x01);

  /*
   * Counter 2, mode 0 (one-shot), binary count
   *
   * NOTE! Mode 2 decrements by two (and then the
   * output is flipped each time, giving the same
   * final output frequency as a decrement-by-one),
   * so mode 0 is much better when looking at the
   * individual counts.
   */
  outb(0x43, 0xb0);

  /* Start at 0xffff */
  outb(0x42, 0xff);
  outb(0x42, 0xff);

  /*
   * The PIT starts counting at the next edge, so we
   * need to delay for a microsecond. The easiest way
   * to do that is to just read back the 16-bit counter
   * once from the PIT.
   */
  pit_verify_msb(0);

  if (pit_expect_msb(0xff, &tsc, &d1)) {
    for (i = 1; i <= (int)MAX_QUICK_PIT_ITERATIONS; i++) {
      if (!pit_expect_msb(0xff - i, &delta, &d2))
        break;

      delta -= tsc;

      /*
       * Extrapolate the error and fail fast if the error will
       * never be below 500 ppm.
       */
      if (i == 1 && d1 + d2 >= (delta * MAX_QUICK_PIT_ITERATIONS) >> 11)
        return 0;

      /*
       * Iterate until the error is less than 500 ppm
       */
      if (d1 + d2 >= delta >> 11)
        continue;

      /*
       * Check the PIT one more time to verify that
       * all TSC reads were stable wrt the PIT.
       *
       * This also guarantees serialization of the
       * last cycle read ('d2') in pit_expect_msb.
       */
      if (!pit_verify_msb(0xfe - i))
        break;
      goto success;
    }
  }
  panic("rtc: TSC calibration failed\n");

success:
  /*
   * Ok, if we get here, then we've seen the
   * MSB of the PIT decrement 'i' times, and the
   * error has shrunk to less than 500 ppm.
   *
   * As a result, we can depend on there not being
   * any odd delays anywhere, and the TSC reads are
   * reliable (within the error).
   *
   * kHz = ticks / time-in-seconds / 1000;
   * kHz = (t2 - t1) / (I * 256 / PIT_TICK_RATE) / 1000
   * kHz = ((t2 - t1) * PIT_TICK_RATE) / (I * 256 * 1000)
   */
  delta *= PIT_TICK_RATE;
  delta /= i * 256 * 1000;
  return delta;
}

#define CMOS_ADDRESS_REGISTER 0x70
#define CMOS_DATA_REGISTER 0x71

#define BASE_EPOCH_YEAR 1970
#define BASE_CENTURY 2000

enum rtc_registers {
  Seconds = 0x00,
  Minutes = 0x02,
  Hours = 0x04,
  WeekDay = 0x06,
  DayOfMonth = 0x07,
  Month = 0x08,
  Year = 0x09,

  StatusRegisterA = 0x0A,
  StatusRegisterB = 0x0B
};

static const uint64_t daysPerMonth[12] = {31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};

static uint8_t read_rtc_register(enum rtc_registers port_number) {
  outb(CMOS_ADDRESS_REGISTER, port_number);
  return inb(CMOS_DATA_REGISTER);
}

static bool is_rtc_updating() {
  uint8_t statusRegisterA = read_rtc_register(StatusRegisterA);
  return statusRegisterA & 0x80;
}

static uint64_t convert_bcd_to_binary(uint8_t value) {
  // The formula below conver a hex number into the decimal version (it meanst:
  // 0x22 will became just 22)
  return ((value / 16) * 10) + (value & 0x0f);
}

static uint64_t read_rtc_time() {
  // Yes is nearly identical to the one in northport! (i implemented it there
  // first and then ported it to Dreamos! :)

  while (is_rtc_updating())
    ;
  const uint8_t rtc_register_statusB = read_rtc_register(StatusRegisterB);
  // Depending on the configuration of the RTC we can have different formats for
  // the time It can be in 12/24 hours format Or it can be in Binary or BCD,
  // binary is the time as it is, BCD use hex values for the time meaning that
  // if read 22:33:12 in BCD it actually is 0x22, 0x33, 0x12, in this case we
  // need to render the numbers in decimal before printing them
  const bool is_24hours = rtc_register_statusB & 0x02;
  const bool is_binary = rtc_register_statusB & 0x04;

  uint8_t seconds = read_rtc_register(Seconds);
  uint8_t minutes = read_rtc_register(Minutes);
  uint8_t hours = read_rtc_register(Hours);
  uint8_t year = read_rtc_register(Year);
  uint8_t dayofmonth = read_rtc_register(DayOfMonth);
  uint8_t month = read_rtc_register(Month);
  uint8_t last_seconds;
  uint8_t last_minutes;
  uint8_t last_hours;
  uint8_t last_year;
  uint8_t last_dayofmonth;
  uint8_t last_month;
  do {
    last_seconds = seconds;
    last_minutes = minutes;
    last_hours = hours;
    last_year = year;
    last_dayofmonth = dayofmonth;
    last_month = month;

    while (is_rtc_updating())
      ;
    seconds = read_rtc_register(Seconds);
    minutes = read_rtc_register(Minutes);
    hours = read_rtc_register(Hours);
    year = read_rtc_register(Year);
    dayofmonth = read_rtc_register(DayOfMonth);
    month = read_rtc_register(Month);
  } while (last_seconds == seconds && last_minutes == minutes &&
           last_hours == hours && last_year == year &&
           last_dayofmonth == dayofmonth && last_month == month);

  if (!is_binary) {
    hours = convert_bcd_to_binary(hours);
    seconds = convert_bcd_to_binary(seconds);
    minutes = convert_bcd_to_binary(minutes);
    year = convert_bcd_to_binary(year);
    month = convert_bcd_to_binary(month);
    dayofmonth = convert_bcd_to_binary(dayofmonth);
  }

  const uint64_t yearsSinceEpoch =
      (BASE_CENTURY + year) - 1970; // Let's count the number of years passed
                                    // since the Epoch Year: (1970)
  uint64_t leapYears =
      yearsSinceEpoch / 4; // We need to know how many leap years too...

  // if yearsSinceEpoch % 4 is greater/equal than 2 we have to add another leap
  // year
  if ((yearsSinceEpoch % 4) > 1) {
    leapYears++;
  }

  uint64_t daysCurrentYear = 0;
  for (int i = 0; i < month - 1; i++) {
    daysCurrentYear += daysPerMonth[i];
  }

  daysCurrentYear = daysCurrentYear + (dayofmonth);
  // If the rtc is set using 12 hours, when the hour indicates PM time
  // The 0x80 bit of the hour is set
  if (!is_24hours && (hours & 0x80)) {
    hours = ((hours & 0x7F) + 12) % 24;
  }

  const uint64_t daysSinceEpoch = (yearsSinceEpoch * 365) - 1;
  const uint64_t unixTimeOfDay = (hours * 3600) + (minutes * 60) + seconds;
  return ((daysSinceEpoch * 86400) + (leapYears * 86400) +
          (daysCurrentYear * 86400) + unixTimeOfDay);
}

/**
 * Each second, the TSC will increment by this amount.
 */
static uint64_t tsc_frequency;
/**
 * The initial RTC time read from CMOS
 */
static uint64_t initial_rtc;
/**
 * Initial TSC value just after reading the initial epoch
 */
static uint64_t initial_tsc;

/**
 * Initializes the RTC component. More especially, it will load the CMOS
 * values to memory and get the frequency of the TSC using the PIC.
 */
void rtc_init(void) {
  tsc_frequency = quick_pit_calibrate();
  initial_rtc = read_rtc_time();
  initial_tsc = get_tsc();
  kprintf("TSC Frequency set to %lluKHz and initial RTC is %llu\n",
          tsc_frequency, initial_rtc);
  initial_rtc *= RTC_PRECISION;
  // Also remember to convert KHz to Hz
  tsc_frequency *= 1000;
  tsc_frequency /= RTC_PRECISION;
}

/**
 * Gets the RTC time in unix epoch, with millisecond precision.
 */
uint64_t rtc_get_epoch(void) {
  return initial_rtc + (get_tsc() - initial_tsc) / tsc_frequency;
}