#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include "keymap.h"

extern "C" {
#include "usbdrv/usbdrv.h"
#include "usbdrv/oddebug.h"
}

#define KEY_CODE_PER_REPORT 5
#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

typedef struct __attribute__((__packed__)) {
  uint8_t reportId;
  uint8_t modifiers;
  uint8_t _unused;
  uint8_t keycodes[KEY_CODE_PER_REPORT];
} kbReport;

#define BUFFER_SIZE sizeof(kbReport)

PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = { /* USB report descriptor */
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,                    // USAGE (Keyboard)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
  0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x95, 0x08,                    //   REPORT_COUNT (8)
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  0x85, 0x01,                    //   REPORT_ID (1)
  0x95, BUFFER_SIZE - 1,         //   REPORT_COUNT (simultaneous keystrokes)
  0x75, 0x08,                    //   REPORT_SIZE (8)
  0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
  0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
  0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
  0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
  0x95, 0x05,                    //   REPORT_COUNT (5)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x05, 0x08,                    //   USAGE_PAGE (LEDs)
  0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
  0x29, 0x02,                    //   USAGE_MAXIMUM (Caps Lock) (We don't care about anything else)
  0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
  0x95, 0x01,                    //   REPORT_COUNT (1)
  0x75, 0x03,                    //   REPORT_SIZE (3)
  0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)
  0xc0                           // END_COLLECTION
};

static kbReport reportBuffer;
static uchar idleRate;
static uchar lastReq;
usbMsgLen_t usbFunctionSetup(uchar data[8]) {
  usbRequest_t *rq = (void *) data;

  /* The following requests are never used. But since they are required by
   * the specification, we implement them in this example.
   */
  if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
    lastReq = rq->bRequest;
    if (rq->bRequest == USBRQ_HID_GET_REPORT) {  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
      /* we only have one report type, so don't look at wValue */
      usbMsgPtr = (void *) &reportBuffer;
      return sizeof(reportBuffer);
    } else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
      return 0;
    } else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
      idleRate = rq->wValue.bytes[1];
    } else if (rq->bRequest == USBRQ_HID_SET_REPORT) {
      // The first byte is report id (we only accept 0x01)
      return rq->wLength.word == 2 ? USB_NO_MSG : 0;
    }
  } else {
    /* no vendor specific requests implemented */
  }
  return 0;   /* default for not implemented requests: return no data back to host */
}

usbMsgLen_t usbFunctionWrite(uchar *data, uchar len) {
  if (lastReq == USBRQ_HID_SET_REPORT && data[0] == 0x01) {
    // (Only accept report ID 0x01 because we have multiple endpoints)
    // The LED state
    // Num Lock
    digitalWrite(12, CHECK_BIT(data[1], 0));
    // Caps Lock
    digitalWrite(13, CHECK_BIT(data[1], 1));
  }
  return 1;
}

const uint8_t rowPinMapping[NUM_ROW_SEL_PINS] = { 0, 1, 5, 6 };
const uint8_t colPinMapping[NUM_COL_IN_PINS] = { 7, 8, 9, 10, 11, A0, A1, A2, A3, A4, A5 };
uint8_t pressedKeys[NUM_ROWS * NUM_COL_IN_PINS];
uint8_t pressedNum = 0;

// ROWs are output pins connected to 4-16 decoder
void initRows() {
  for (uint8_t i = 0; i < NUM_ROW_SEL_PINS; i++) {
    pinMode(rowPinMapping[i], OUTPUT);
    digitalWrite(rowPinMapping[i], LOW);
  }
}

// COLs are input pins pulled up by default
// accepting input through switches from the 4-16 decoder
void initCols() {
  for (uint8_t i = 0; i < NUM_COL_IN_PINS; i++) {
    pinMode(colPinMapping[i], INPUT_PULLUP);
  }
}

void scanRow(uint8_t row) {
  // Send data to the 4-16 decoder to set the corresponding row to LOW
  for (uint8_t i = 0; i < NUM_ROW_SEL_PINS; i++) {
    digitalWrite(rowPinMapping[i], CHECK_BIT(row, i));
  }
  // Read from all input pins to determine which ones are pressed
  // loop over all cols
  for (uint8_t i = 0; i < NUM_COL_IN_PINS; i++) {
    if (!digitalRead(colPinMapping[i])) {
      // TODO: implement modifier keys
      pressedKeys[pressedNum] = keymap[row][i];
      pressedNum++;
    }
  }
}

void scan() {
  pressedNum = 0;
  for (uint8_t i = 0; i < NUM_ROWS; i++)
    scanRow(i);
}

// TODO: implement multiple report buffers
void fillReportBuffer() {
  memset((void *) &reportBuffer, 0, sizeof(reportBuffer));
  reportBuffer.reportId = 0x01;
  uint8_t startNum = 0;
  uint8_t maxNum = startNum + 5;
  if (startNum >= pressedNum)
    return;
  if (maxNum > pressedNum)
    maxNum = pressedNum;
  for (uint8_t i = startNum; i < maxNum; i++) {
    reportBuffer.keycodes[i - startNum] = pressedKeys[i];
  }
}

void setup() {
  // Meta state pins
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  // Initialize key scanning infrastructure
  initRows();
  initCols();
  // USB protocol initialization
  cli();
  usbInit();
  usbDeviceDisconnect();
  _delay_ms(260);
  usbDeviceConnect();
  sei();
}

void loop() {
  usbPoll();
  if (usbInterruptIsReady()) {
    // Always scan the entire keyboard when interrupt is ready
    scan();
    fillReportBuffer();
    usbSetInterrupt((void *) &reportBuffer, sizeof(reportBuffer));
  }
}
