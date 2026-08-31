import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, wifi
from esphome.const import CONF_ID


DEPENDENCIES = ["wifi"]
CODEOWNERS = []

CONF_WIFI_ID = "wifi_id"
CONF_RELAYS = "relays"

persistent_web_portal_ns = cg.esphome_ns.namespace("persistent_web_portal")
PersistentWebPortal = persistent_web_portal_ns.class_(
    "PersistentWebPortal", cg.Component
)


def _exactly_four_relays(value):
    value = cv.ensure_list(cv.use_id(switch.Switch))(value)
    if len(value) != 4:
        raise cv.Invalid("exactly four relay switch IDs are required")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PersistentWebPortal),
            cv.GenerateID(CONF_WIFI_ID): cv.use_id(wifi.WiFiComponent),
            cv.Required(CONF_RELAYS): _exactly_four_relays,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    cv.only_with_arduino,
)


async def to_code(config):
    # ESPHome disables optional Arduino libraries unless a component asks for
    # them. The portal uses the synchronous HTTP/DNS APIs, while ESPHome keeps
    # ownership of the underlying ESP-IDF Wi-Fi driver.
    for library in ("Network", "WiFi", "FS", "WebServer", "AsyncUDP", "DNSServer"):
        cg.add_library(library, None)

    wifi.request_wifi_scan_results_listener()
    wifi.request_wifi_connect_state_listener()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    wifi_var = await cg.get_variable(config[CONF_WIFI_ID])
    cg.add(var.set_wifi(wifi_var))

    for relay_id in config[CONF_RELAYS]:
        relay = await cg.get_variable(relay_id)
        cg.add(var.add_relay(relay))
