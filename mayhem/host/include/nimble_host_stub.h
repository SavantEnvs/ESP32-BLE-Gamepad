// Host-side stand-in for the parts of Arduino-ESP32 and NimBLE-Arduino that
// ESP32-BLE-Gamepad uses. NimBLE needs the ESP32 BLE controller and cannot be
// built for a Linux host, so this models only the GATT surface the library
// talks to: characteristics hold a value in an exact-size heap buffer, record
// the callbacks the library installs, and let the harness deliver host
// writes/reads through those callbacks (the same path the BLE stack uses).
#ifndef NIMBLE_HOST_STUB_H
#define NIMBLE_HOST_STUB_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

#define CONFIG_BT_ENABLED 1
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL 1
#ifndef CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 3
#endif

// ---------------------------------------------------------------- logging
#define NIMBLE_LOGD(tag, ...) do { (void)(tag); } while (0)
#define NIMBLE_LOGI(tag, ...) do { (void)(tag); } while (0)
#define NIMBLE_LOGW(tag, ...) do { (void)(tag); } while (0)
#define NIMBLE_LOGE(tag, ...) do { (void)(tag); } while (0)
#define NIMBLE_LOGC(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGD(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGE(tag, ...) do { (void)(tag); } while (0)

// ---------------------------------------------------------------- Arduino
class String : public std::string
{
  public:
    String() {}
    String(const char *s) : std::string(s ? s : "") {}
    String(const std::string &s) : std::string(s) {}
};

struct HostSerial
{
    operator bool() const { return true; }
    void println(const char *) {}
    void println() {}
    void print(const char *) {}
    int printf(const char *, ...) { return 0; }
};
extern HostSerial Serial;

struct HostEsp
{
    void restart() {}
};
extern HostEsp ESP;

#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
void delay(unsigned long ms);
unsigned long millis();

// ---------------------------------------------------------------- FreeRTOS
typedef void (*TaskFunction_t)(void *);
typedef void *TaskHandle_t;
typedef uint32_t TickType_t;
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
// Runs the task body synchronously on the caller's thread.
int xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *param, unsigned prio, TaskHandle_t *handle);
void vTaskDelay(TickType_t ticks);

// ---------------------------------------------------------------- HID constants
#define HID_GAMEPAD 0x03C4
#define BLE_GAP_LE_PHY_1M_MASK 0x01
#define BLE_GAP_LE_PHY_2M_MASK 0x02

namespace NIMBLE_PROPERTY
{
enum : uint32_t
{
    READ = 0x0002,
    READ_ENC = 0x0200,
    READ_AUTHEN = 0x0400,
    READ_AUTHOR = 0x0800,
    WRITE = 0x0008,
    WRITE_NR = 0x0004,
    WRITE_ENC = 0x1000,
    WRITE_AUTHEN = 0x2000,
    WRITE_AUTHOR = 0x4000,
    BROADCAST = 0x0001,
    NOTIFY = 0x0010,
    INDICATE = 0x0020,
};
}

// Every stub object registers itself so stub_reset() can free what the
// library allocated (it never frees its NimBLEHIDDevice/characteristics).
struct StubObject
{
    StubObject();
    virtual ~StubObject() {}
};

class NimBLEUUID
{
  public:
    NimBLEUUID() {}
    NimBLEUUID(uint16_t v) : value(v) {}
    NimBLEUUID(const char *s) : value((uint16_t)strtoul(s ? s : "0", nullptr, 16)) {}
    NimBLEUUID(const std::string &s) : NimBLEUUID(s.c_str()) {}
    bool operator==(const NimBLEUUID &o) const { return value == o.value; }
    uint16_t value = 0;
};

class NimBLEAddress
{
  public:
    NimBLEAddress() {}
    NimBLEAddress(const std::string &s, uint8_t type) : str(s), type(type) {}
    std::string toString() const { return str; }
    operator std::string() const { return str; }
    bool operator==(const NimBLEAddress &o) const { return str == o.str; }
    bool operator!=(const NimBLEAddress &o) const { return str != o.str; }
    bool isNull() const { return str.empty() || str == "00:00:00:00:00:00"; }
    std::string str = "00:00:00:00:00:00";
    uint8_t type = 0;
};

class NimBLEConnInfo
{
  public:
    NimBLEAddress getAddress() const { return address; }
    NimBLEAddress getIdAddress() const { return address; }
    uint16_t getConnHandle() const { return connHandle; }
    bool isEncrypted() const { return encrypted; }
    bool isBonded() const { return bonded; }
    bool isAuthenticated() const { return encrypted; }
    uint16_t getMTU() const { return 247; }
    NimBLEAddress address{"11:22:33:44:55:66", 0};
    uint16_t connHandle = 1;
    bool encrypted = false;
    bool bonded = false;
};

// Attribute value: an exact-size heap copy so ASan sees any over-read.
class NimBLEAttValue
{
  public:
    NimBLEAttValue() {}
    NimBLEAttValue(const uint8_t *d, size_t n) { assign(d, n); }
    NimBLEAttValue(const NimBLEAttValue &o) { assign(o.buf, o.len); }
    NimBLEAttValue &operator=(const NimBLEAttValue &o)
    {
        if (this != &o) assign(o.buf, o.len);
        return *this;
    }
    ~NimBLEAttValue() { delete[] buf; }
    void assign(const uint8_t *d, size_t n)
    {
        uint8_t *nb = new uint8_t[n ? n : 1];
        if (n) memcpy(nb, d, n);
        delete[] buf;
        buf = nb;
        len = n;
    }
    const uint8_t *data() const { return buf; }
    size_t size() const { return len; }
    size_t length() const { return len; }
    operator std::string() const { return std::string((const char *)buf, len); }
    const char *c_str() const = delete;

  private:
    uint8_t *buf = nullptr;
    size_t len = 0;
};

class NimBLECharacteristic;
class NimBLEServer;

class NimBLECharacteristicCallbacks
{
  public:
    virtual ~NimBLECharacteristicCallbacks() {}
    virtual void onRead(NimBLECharacteristic *, NimBLEConnInfo &) {}
    virtual void onWrite(NimBLECharacteristic *, NimBLEConnInfo &) {}
    virtual void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &, uint16_t) {}
    virtual void onStatus(NimBLECharacteristic *, int) {}
};

class NimBLEServerCallbacks
{
  public:
    virtual ~NimBLEServerCallbacks() {}
    virtual void onConnect(NimBLEServer *, NimBLEConnInfo &) {}
    virtual void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) {}
    virtual void onAuthenticationComplete(NimBLEConnInfo &) {}
    virtual void onMTUChange(uint16_t, NimBLEConnInfo &) {}
};

class NimBLEDescriptor : public StubObject
{
  public:
    NimBLEDescriptor(NimBLEUUID u) : uuid(u) {}
    void setValue(const uint8_t *d, size_t n) { value.assign(d, n); }
    NimBLEUUID uuid;
    NimBLEAttValue value;
};

enum StubReportType : uint8_t
{
    STUB_PLAIN = 0,
    STUB_INPUT = 1,
    STUB_OUTPUT = 2,
    STUB_FEATURE = 3,
};

class NimBLECharacteristic : public StubObject
{
  public:
    NimBLECharacteristic(NimBLEUUID u, uint32_t props);
    void setValue(const uint8_t *d, size_t n) { value.assign(d, n); }
    void setValue(const std::string &s) { value.assign((const uint8_t *)s.data(), s.size()); }
    template <typename T> void setValue(const T &v) { value.assign((const uint8_t *)&v, sizeof(T)); }
    NimBLEAttValue getValue() const { return value; }
    bool notify(uint16_t connHandle = 0xffff);
    bool indicate() { return notify(); }
    void setCallbacks(NimBLECharacteristicCallbacks *cb) { callbacks = cb; }
    NimBLECharacteristicCallbacks *getCallbacks() const { return callbacks; }
    NimBLEDescriptor *createDescriptor(NimBLEUUID u, uint32_t props = 0, uint16_t maxLen = 100);
    NimBLEUUID getUUID() const { return uuid; }

    NimBLEUUID uuid;
    uint32_t properties;
    StubReportType reportType = STUB_PLAIN;
    uint8_t reportId = 0;
    NimBLEAttValue value;
    NimBLECharacteristicCallbacks *callbacks = nullptr;
    size_t notifyCount = 0;
};
typedef NimBLECharacteristic BLECharacteristic;

class NimBLEService : public StubObject
{
  public:
    NimBLEService(NimBLEUUID u) : uuid(u) {}
    NimBLECharacteristic *createCharacteristic(NimBLEUUID u, uint32_t props = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, uint16_t maxLen = 512);
    NimBLECharacteristic *createCharacteristic(const char *u, uint32_t props = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, uint16_t maxLen = 512)
    {
        return createCharacteristic(NimBLEUUID(u), props, maxLen);
    }
    NimBLEUUID getUUID() const { return uuid; }
    bool start() { return true; }
    NimBLEUUID uuid;
};

class NimBLEAdvertising : public StubObject
{
  public:
    bool setAppearance(uint16_t a) { appearance = a; return true; }
    bool setName(const std::string &n) { name = n; return true; }
    bool addServiceUUID(const NimBLEUUID &) { return true; }
    bool start(uint32_t = 0) { advertising = true; return true; }
    bool stop() { advertising = false; return true; }
    bool isAdvertising() const { return advertising; }
    uint16_t appearance = 0;
    std::string name;
    bool advertising = false;
};

class NimBLEServer : public StubObject
{
  public:
    void setCallbacks(NimBLEServerCallbacks *cb, bool = true) { callbacks = cb; }
    void advertiseOnDisconnect(bool) {}
    NimBLEAdvertising *getAdvertising();
    NimBLEConnInfo getPeerInfo(size_t) const { return peer; }
    std::vector<uint16_t> getPeerDevices() const { return connected ? std::vector<uint16_t>{peer.connHandle} : std::vector<uint16_t>{}; }
    bool disconnect(uint16_t, uint8_t = 0x13) { return true; }
    NimBLEService *getServiceByUUID(const char *u);
    NimBLEService *getServiceByUUID(const NimBLEUUID &u);
    NimBLEService *createService(const NimBLEUUID &u);
    void updateConnParams(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t) {}
    size_t getConnectedCount() const { return connected ? 1 : 0; }
    bool start() { return true; }

    NimBLEServerCallbacks *callbacks = nullptr;
    NimBLEAdvertising *advertising = nullptr;
    std::vector<NimBLEService *> services;
    NimBLEConnInfo peer;
    bool connected = false;
};

class NimBLEHIDDevice : public StubObject
{
  public:
    NimBLEHIDDevice(NimBLEServer *server);
    NimBLECharacteristic *getInputReport(uint8_t id) { return report(STUB_INPUT, id); }
    NimBLECharacteristic *getOutputReport(uint8_t id) { return report(STUB_OUTPUT, id); }
    NimBLECharacteristic *getFeatureReport(uint8_t id) { return report(STUB_FEATURE, id); }
    NimBLEService *getHidService() { return hidService; }
    NimBLEService *getBatteryService() { return batteryService; }
    NimBLEService *getDeviceInfoService() { return deviceInfoService; }
    void setReportMap(uint8_t *map, uint16_t size) { reportMap.assign(map, size); }
    bool setManufacturer(const std::string &m) { manufacturer = m; return true; }
    void setPnp(uint8_t, uint16_t, uint16_t, uint16_t) {}
    void setHidInfo(uint8_t, uint8_t) {}
    void setBatteryLevel(uint8_t level, bool notify = false) { batteryLevel = level; (void)notify; }

    NimBLECharacteristic *report(StubReportType type, uint8_t id);
    NimBLEService *hidService;
    NimBLEService *batteryService;
    NimBLEService *deviceInfoService;
    NimBLEAttValue reportMap;
    std::string manufacturer;
    uint8_t batteryLevel = 0;
};

class NimBLEDevice
{
  public:
    static bool init(const std::string &name);
    static bool setPower(int8_t dbm);
    static int getPower();
    static bool setDefaultPhy(uint8_t, uint8_t) { return true; }
    static NimBLEServer *createServer();
    static NimBLEServer *getServer();
    static void setSecurityAuth(bool, bool, bool) {}
    static bool deleteBond(const NimBLEAddress &) { return true; }
    static bool deleteAllBonds() { return true; }
};

// ---------------------------------------------------------------- harness API
// Frees every stub object and forgets the server (call after BleGamepad::end()).
void stub_reset();
// All characteristics created since the last reset, in creation order.
const std::vector<NimBLECharacteristic *> &stub_characteristics();
// First characteristic of the given HID report type that has callbacks installed.
NimBLECharacteristic *stub_find(StubReportType type);
// First characteristic created writable (WRITE/WRITE_NR) with callbacks installed.
NimBLECharacteristic *stub_find_writable();
// Deliver a host write / read through the installed callbacks.
void stub_host_write(NimBLECharacteristic *c, const uint8_t *data, size_t len);
NimBLEAttValue stub_host_read(NimBLECharacteristic *c);
// Simulate a bonded central connecting / disconnecting.
void stub_host_connect(bool encrypted);
void stub_host_disconnect();
// Total bytes passed through notify() since reset (each notify reads the value).
size_t stub_notified_bytes();

#endif
