package daemon

import (
	"context"
	"encoding/json"
	"fmt"
	"testing"

	"github.com/chr0nzz/omarchy-alienware/helper/internal/backend"
	"github.com/chr0nzz/omarchy-alienware/helper/internal/hw"
)

func TestLightingAuthorizationBeforeNativeWrite(t *testing.T) {
	calls := 0
	r := &hw.Reader{Root: t.TempDir(), Backend: &backend.Client{Run: func(context.Context, []byte) ([]byte, error) {
		calls++
		return []byte(`{"ok":true,"devices":[]}`), nil
	}}}
	s := NewService(r, denyAll{}, quietLogger())
	if _, err := s.LightingRequest(`{"section":"device","action":"set-all","color":"FF0000"}`, ":1.1"); err == nil || err.Name != ErrNameDenied {
		t.Fatalf("write bypassed authorization: %v", err)
	}
	if calls != 0 {
		t.Fatal("unauthorized write reached native backend")
	}
	if _, err := s.LightingRequest(`{"section":"device","action":"status"}`, ":1.1"); err != nil {
		t.Fatal(err)
	}
	if calls != 1 {
		t.Fatal("read-only inventory did not reach backend")
	}
	for _, payload := range []string{`null`, `not json`, `{"section":"thermal","action":"boost"}`, `{"section":"rgb","action":"reset"}`} {
		if _, err := s.LightingRequest(payload, ":1.1"); err == nil || err.Name != ErrNameBadRequest {
			t.Fatalf("invalid request accepted: %s (%v)", payload, err)
		}
	}
	if calls != 1 {
		t.Fatal("invalid request reached native backend")
	}
}

func TestCurveRequiresAdvertisedCapabilityAndSuccessfulCustomProfile(t *testing.T) {
	for _, capable := range []bool{false, true} {
		actions := []string{}
		r := &hw.Reader{Root: t.TempDir(), Backend: &backend.Client{Run: func(_ context.Context, raw []byte) ([]byte, error) {
			var request map[string]any
			if err := json.Unmarshal(raw, &request); err != nil {
				t.Fatal(err)
			}
			action := request["action"].(string)
			actions = append(actions, action)
			if action == "profile" {
				return []byte(`{"ok":false,"error":"firmware refused custom"}`), nil
			}
			return []byte(fmt.Sprintf(`{"ok":true,"curveSupported":%t,"profile":{"current":"balanced","choices":["balanced","custom"]}}`, capable)), nil
		}}}
		s := NewService(r, nil, quietLogger())
		if err := s.StartCurve(hotCurve()); err == nil {
			t.Fatal("unsafe curve started")
		}
		if s.curveStatus().Active {
			t.Fatal("curve became active after rejected capability/profile")
		}
		for _, action := range actions {
			if action == "boost" {
				t.Fatal("fan boost written before selecting custom profile")
			}
			if !capable && action != "status" {
				t.Fatal("unadvertised curve issued a write")
			}
		}
	}
}

func TestManualWritesCannotRaceActiveCurve(t *testing.T) {
	r := &hw.Reader{Backend: &backend.Client{Run: func(context.Context, []byte) ([]byte, error) {
		t.Fatal("manual write reached backend while curve was active")
		return nil, nil
	}}}
	s := NewService(r, nil, quietLogger())
	s.active = true
	if err := s.SetProfile("performance", ":1.1"); err == nil || err.Name != ErrNameBadRequest {
		t.Fatalf("profile race: %v", err)
	}
	if err := s.SetBoost("cpu", 40, ":1.1"); err == nil || err.Name != ErrNameBadRequest {
		t.Fatalf("boost race: %v", err)
	}
}
