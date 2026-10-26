package cli

import "testing"

func TestDiscoveredFanIDs(t *testing.T) {
	for _, id := range []string{"cpu", "gpu", "fan1", "fan3", "fan255"} {
		if !validFanID(id) {
			t.Errorf("rejected %q", id)
		}
	}
	for _, id := range []string{"fan0", "fan256", "fan01", "fan-1", "fan+1", "fan", "CPU", "cpu;sh"} {
		if validFanID(id) {
			t.Errorf("accepted %q", id)
		}
	}
}

func TestSDKLightingArgumentsAreValidatedBeforeDBus(t *testing.T) {
	for _, args := range [][]string{{"status", "extra"}, {"set-all", "nope"}, {"brightness", "-1"}, {"brightness", "101"}, {"set-map", "foo=ff0000,foo=00ff00"}, {"reset"}, {"mode", "guess"}} {
		if _, err := sdkRGBRequest(args); err == nil {
			t.Errorf("accepted %v", args)
		}
	}
	for _, args := range [][]string{{"status"}, {"off"}, {"set-all", "FF0000"}, {"brightness", "0"}, {"set", "verified-zone", "00FF00"}, {"set-map", "zone=ff0000"}} {
		if _, err := sdkRGBRequest(args); err != nil {
			t.Errorf("rejected %v: %v", args, err)
		}
	}
}
