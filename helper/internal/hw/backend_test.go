package hw

import (
	"context"
	"encoding/json"
	"github.com/chr0nzz/omarchy-alienware/helper/internal/backend"
	"os"
	"path/filepath"
	"testing"
)

func TestBackendSnapshotAndWriteStayOnSelectedBackend(t *testing.T) {
	var calls []map[string]any
	r := &Reader{Root: t.TempDir(), RunNvidia: func() (string, error) { return "", nil }}
	r.Backend = &backend.Client{Run: func(_ context.Context, raw []byte) ([]byte, error) {
		var request map[string]any
		if err := json.Unmarshal(raw, &request); err != nil {
			t.Fatal(err)
		}
		calls = append(calls, request)
		return []byte(`{"ok":true,"backend":"awcc","supportedModel":true,"curveSupported":false,"fans":[{"id":"cpu","readable":false}],"profile":{"current":"balanced","choices":["balanced"]}}`), nil
	}}
	snap := r.Snapshot()
	if snap.Backend.Name != "awcc" || len(snap.Profile.Choices) != 1 || snap.Fans[0].Readable == nil || *snap.Fans[0].Readable {
		t.Fatalf("lost backend capability: %+v", snap)
	}
	if r.CanCurve() {
		t.Fatal("AWCC advertised as additive kernel curve")
	}
	if err := r.SetProfile("balanced"); err != nil {
		t.Fatal(err)
	}
	if calls[len(calls)-1]["action"] != "profile" {
		t.Fatal(calls)
	}
	if err := r.SetBoost("cpu", 30); err != nil {
		t.Fatal(err)
	}
	if calls[len(calls)-1]["value"] != float64(30) {
		t.Fatal(calls)
	}
}

func TestAMDGenericTurboIsNotIntelInverted(t *testing.T) {
	r := &Reader{Root: t.TempDir()}
	file := filepath.Join(r.Root, "sys/devices/system/cpu/cpufreq/boost")
	if err := os.MkdirAll(filepath.Dir(file), 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(file, []byte("1"), 0644); err != nil {
		t.Fatal(err)
	}
	if got := r.readTurbo(&warnings{}); !got.Available || !got.Enabled {
		t.Fatal(got)
	}
	if err := r.SetTurbo(false); err != nil {
		t.Fatal(err)
	}
	data, _ := os.ReadFile(file)
	if string(data) != "0" {
		t.Fatal(string(data))
	}
}

func TestFailedBackendSnapshotCannotLookLikeHealthyMissingHardware(t *testing.T) {
	r := &Reader{Root: t.TempDir(), Backend: &backend.Client{Run: func(context.Context, []byte) ([]byte, error) {
		return []byte(`{"ok":false,"code":"device-busy"}`), nil
	}}}
	if snapshot := r.Snapshot(); snapshot.BackendError == nil {
		t.Fatal("failed native status looked healthy")
	}
}
