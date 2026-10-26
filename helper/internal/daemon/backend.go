package daemon

import (
	"encoding/json"
	"github.com/godbus/dbus/v5"
)

func (s *Service) LightingRequest(payload string, sender dbus.Sender) (string, *dbus.Error) {
	if len(payload) > 65536 {
		return "", dbusError(ErrNameBadRequest, "lighting request too large")
	}
	var request map[string]any
	if err := json.Unmarshal([]byte(payload), &request); err != nil {
		return "", dbusError(ErrNameBadRequest, "invalid lighting request")
	}
	section, _ := request["section"].(string)
	action, _ := request["action"].(string)
	if section != "rgb" && section != "kbd" && section != "device" {
		return "", dbusError(ErrNameBadRequest, "invalid lighting section")
	}
	if action != "status" && action != "set-map" && action != "set-all" && action != "off" && action != "brightness" {
		return "", dbusError(ErrNameBadRequest, "unsupported lighting action")
	}
	if action != "status" {
		if err := s.auth.Authorize(sender, ActionSetKeyboard); err != nil {
			return "", mapError(err)
		}
	}
	if s.reader.Backend == nil {
		return "", dbusError(ErrNameNotSupported, "SDK backend is not installed; rebuild the helper from this checkout")
	}
	out, err := s.reader.Backend.Call(request)
	if err != nil {
		return "", mapError(err)
	}
	return string(out), nil
}
