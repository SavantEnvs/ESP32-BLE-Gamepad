// Implementation of the host-side NimBLE/Arduino stand-in (see include/nimble_host_stub.h).
#include "nimble_host_stub.h"

HostSerial Serial;
HostEsp ESP;

static std::vector<StubObject *> g_objects;
static std::vector<NimBLECharacteristic *> g_chars;
static NimBLEServer *g_server = nullptr;
static size_t g_notified = 0;
static int8_t g_power = 0;
static unsigned long g_millis = 0;

StubObject::StubObject() { g_objects.push_back(this); }

void delay(unsigned long ms) { g_millis += ms; }
unsigned long millis() { return ++g_millis; }

int xTaskCreate(TaskFunction_t fn, const char *, uint32_t, void *param, unsigned, TaskHandle_t *handle)
{
    if (handle) *handle = nullptr;
    fn(param);
    return 1;
}
void vTaskDelay(TickType_t) {}

NimBLECharacteristic::NimBLECharacteristic(NimBLEUUID u, uint32_t props) : uuid(u), properties(props)
{
    g_chars.push_back(this);
}

bool NimBLECharacteristic::notify(uint16_t)
{
    // A real stack copies the whole value into an ATT PDU; touch every byte.
    volatile uint8_t sink = 0;
    for (size_t i = 0; i < value.size(); i++) sink ^= value.data()[i];
    (void)sink;
    g_notified += value.size();
    notifyCount++;
    return true;
}

NimBLEDescriptor *NimBLECharacteristic::createDescriptor(NimBLEUUID u, uint32_t, uint16_t)
{
    return new NimBLEDescriptor(u);
}

NimBLECharacteristic *NimBLEService::createCharacteristic(NimBLEUUID u, uint32_t props, uint16_t)
{
    return new NimBLECharacteristic(u, props);
}

NimBLEAdvertising *NimBLEServer::getAdvertising()
{
    if (!advertising) advertising = new NimBLEAdvertising();
    return advertising;
}

NimBLEService *NimBLEServer::createService(const NimBLEUUID &u)
{
    NimBLEService *s = new NimBLEService(u);
    services.push_back(s);
    return s;
}

NimBLEService *NimBLEServer::getServiceByUUID(const NimBLEUUID &u)
{
    for (NimBLEService *s : services)
        if (s->uuid == u) return s;
    return nullptr;
}

NimBLEService *NimBLEServer::getServiceByUUID(const char *u) { return getServiceByUUID(NimBLEUUID(u)); }

NimBLEHIDDevice::NimBLEHIDDevice(NimBLEServer *server)
{
    deviceInfoService = server->createService(NimBLEUUID((uint16_t)0x180A));
    hidService = server->createService(NimBLEUUID((uint16_t)0x1812));
    batteryService = server->createService(NimBLEUUID((uint16_t)0x180F));
}

NimBLECharacteristic *NimBLEHIDDevice::report(StubReportType type, uint8_t id)
{
    uint32_t props = NIMBLE_PROPERTY::READ;
    if (type == STUB_INPUT) props |= NIMBLE_PROPERTY::NOTIFY;
    if (type == STUB_OUTPUT) props |= NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR;
    if (type == STUB_FEATURE) props |= NIMBLE_PROPERTY::WRITE;
    NimBLECharacteristic *c = hidService->createCharacteristic(NimBLEUUID((uint16_t)0x2A4D), props);
    c->reportType = type;
    c->reportId = id;
    uint8_t ref[2] = {id, (uint8_t)type};
    c->createDescriptor(NimBLEUUID((uint16_t)0x2908))->setValue(ref, 2);
    return c;
}

bool NimBLEDevice::init(const std::string &) { return true; }
bool NimBLEDevice::setPower(int8_t dbm) { g_power = dbm; return true; }
int NimBLEDevice::getPower() { return g_power; }
NimBLEServer *NimBLEDevice::createServer()
{
    if (!g_server) g_server = new NimBLEServer();
    return g_server;
}
NimBLEServer *NimBLEDevice::getServer() { return g_server; }

void stub_reset()
{
    for (StubObject *o : g_objects) delete o;
    g_objects.clear();
    g_chars.clear();
    g_server = nullptr;
    g_notified = 0;
    g_power = 0;
    g_millis = 0;
}

const std::vector<NimBLECharacteristic *> &stub_characteristics() { return g_chars; }

NimBLECharacteristic *stub_find(StubReportType type)
{
    for (NimBLECharacteristic *c : g_chars)
        if (c->reportType == type && c->callbacks) return c;
    return nullptr;
}

NimBLECharacteristic *stub_find_writable()
{
    for (NimBLECharacteristic *c : g_chars)
        if ((c->properties & (NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)) && c->callbacks) return c;
    return nullptr;
}

void stub_host_write(NimBLECharacteristic *c, const uint8_t *data, size_t len)
{
    if (!c) return;
    c->value.assign(data, len);
    NimBLEConnInfo info;
    if (c->callbacks) c->callbacks->onWrite(c, info);
}

NimBLEAttValue stub_host_read(NimBLECharacteristic *c)
{
    if (!c) return NimBLEAttValue();
    NimBLEConnInfo info;
    if (c->callbacks) c->callbacks->onRead(c, info);
    return c->value;
}

void stub_host_connect(bool encrypted)
{
    if (!g_server) return;
    g_server->peer.encrypted = encrypted;
    g_server->peer.bonded = encrypted;
    if (g_server->callbacks)
    {
        g_server->callbacks->onConnect(g_server, g_server->peer);
        if (!encrypted) g_server->callbacks->onAuthenticationComplete(g_server->peer);
    }
    g_server->connected = true;
}

void stub_host_disconnect()
{
    if (!g_server) return;
    if (g_server->callbacks) g_server->callbacks->onDisconnect(g_server, g_server->peer, 0x13);
    g_server->connected = false;
}

size_t stub_notified_bytes() { return g_notified; }
