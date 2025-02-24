from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers import discovery
from homeassistant.helpers.typing import ConfigType
from homeassistant.const import Platform
from .cover import SmartCurtainCover

PLATFORMS = [Platform.COVER]

async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    return True

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True

async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)

# https://github.com/home-assistant/example-custom-config/blob/master/custom_components/detailed_hello_world_push
# https://developers.home-assistant.io/docs/core/entity/cover/
