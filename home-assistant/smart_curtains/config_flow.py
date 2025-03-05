import logging
from homeassistant import config_entries
from homeassistant.core import HomeAssistant
import voluptuous as vol
from homeassistant.helpers import config_validation as cv

_LOGGER = logging.getLogger(__name__)

DOMAIN = "smart_curtains"

class SmartCuratinsConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    def __init__(self):
        self._url = None
        self._name = None

    async def async_step_user(self, user_input=None):
        if user_input is not None:
            self._url = user_input["url"]
            self._name = user_input["name"]

            return self.async_create_entry(
                title=self._name, data={
                    "url": self._url,
                    "name": self._name,
                }
            )

        return self.async_show_form(
            step_id="user", data_schema=vol.Schema({
                vol.Required("url"): str,
                vol.Required("name", default="Smart Curtain"): str,
            })
        )

