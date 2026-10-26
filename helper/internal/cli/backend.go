package cli

import (
	"encoding/json"
	"github.com/chr0nzz/omarchy-alienware/helper/internal/client"
	"strconv"
	"strings"
)

func validFanID(id string) bool {
	if id == "cpu" || id == "gpu" {
		return true
	}
	if !strings.HasPrefix(id, "fan") {
		return false
	}
	n, err := strconv.Atoi(strings.TrimPrefix(id, "fan"))
	return err == nil && n >= 1 && n <= 255 && id == "fan"+strconv.Itoa(n)
}

func lightingRequest(env Env, request map[string]any) int {
	c, err := client.Connect()
	if err != nil {
		return emitError(env.Stdout, err)
	}
	raw, err := json.Marshal(request)
	if err != nil {
		return emitError(env.Stdout, err)
	}
	out, err := c.LightingRequest(string(raw))
	if err != nil {
		return emitError(env.Stdout, err)
	}
	return emitRaw(env.Stdout, out)
}

func sdkRGBRequest(args []string) (map[string]any, error) {
	if len(args) == 0 {
		return nil, badRequest("rgb requires an action")
	}
	r := map[string]any{"section": "rgb", "action": args[0]}
	switch args[0] {
	case "status", "off":
		if len(args) != 1 {
			return nil, badRequest("unexpected lighting arguments")
		}
	case "set-all":
		if len(args) != 2 {
			return nil, badRequest("set-all requires RRGGBB")
		}
		rr, gg, bb, err := parseHexColor(args[1])
		if err != nil {
			return nil, err
		}
		r["color"] = hexColor(rr, gg, bb)
	case "brightness":
		if len(args) != 2 {
			return nil, badRequest("brightness requires 0-100")
		}
		value, err := strconv.Atoi(args[1])
		if err != nil || value < 0 || value > 100 {
			return nil, badRequest("brightness requires 0-100")
		}
		r["value"] = value
	case "set", "set-map":
		values := ""
		if args[0] == "set" && len(args) == 3 {
			values = args[1] + "=" + args[2]
		} else if args[0] == "set-map" && len(args) == 2 {
			values = args[1]
		} else {
			return nil, badRequest("expected region and RRGGBB colors")
		}
		colors := map[string]string{}
		for _, value := range strings.Split(values, ",") {
			pair := strings.SplitN(value, "=", 2)
			if len(pair) != 2 || pair[0] == "" || len(pair[0]) > 64 {
				return nil, badRequest("invalid region color")
			}
			if _, exists := colors[pair[0]]; exists {
				return nil, badRequest("duplicate region")
			}
			rr, gg, bb, err := parseHexColor(pair[1])
			if err != nil {
				return nil, err
			}
			colors[pair[0]] = hexColor(rr, gg, bb)
		}
		r["action"], r["colors"] = "set-map", colors
	default:
		return nil, notSupported("SDK lighting supports status, set, set-map, set-all, brightness and off; it never probes or resets hardware automatically")
	}
	return r, nil
}

func runSDKRGB(env Env, args []string) int {
	request, err := sdkRGBRequest(args)
	if err != nil {
		return emitError(env.Stdout, err)
	}
	return lightingRequest(env, request)
}

func runBackend(env Env, args []string) int {
	if len(args) == 1 && args[0] == "status" {
		return lightingRequest(env, map[string]any{"section": "device", "action": "status"})
	}
	if len(args) == 3 && args[0] == "color" {
		rr, gg, bb, err := parseHexColor(args[2])
		if err != nil {
			return emitError(env.Stdout, err)
		}
		return lightingRequest(env, map[string]any{"section": "device", "action": "set-all", "device": strings.ToLower(args[1]), "color": hexColor(rr, gg, bb)})
	}
	return emitError(env.Stdout, badRequest("backend takes status or color <VID:PID> <RRGGBB>"))
}
