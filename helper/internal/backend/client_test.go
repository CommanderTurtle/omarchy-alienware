package backend

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"testing"
)

func TestReplyValidation(t *testing.T) {
	for _, input := range []struct {
		reply string
		err   error
		good  bool
	}{
		{`{"ok":true}`, nil, true},
		{`{"ok":false,"error":"unsupported model"}`, nil, false},
		{`broken`, nil, false},
		{`{"ok":true}`, fmt.Errorf("failed exit"), false},
		{strings.Repeat("x", Limit+1), nil, false},
	} {
		c := &Client{Run: func(ctx context.Context, raw []byte) ([]byte, error) {
			if _, set := ctx.Deadline(); !set {
				t.Fatal("missing hardware deadline")
			}
			return []byte(input.reply), input.err
		}}
		_, err := c.Call(map[string]any{"section": "thermal", "action": "status"})
		if (err == nil) != input.good {
			t.Errorf("reply %q: %v", input.reply[:min(len(input.reply), 70)], err)
		}
	}
}

func TestCompetingControllerBlocksWritesNotReadOnlyStatus(t *testing.T) {
	calls := 0
	c := &Client{Conflict: func() bool { return true }, Run: func(context.Context, []byte) ([]byte, error) { calls++; return []byte(`{"ok":true}`), nil }}
	if _, err := c.Call(map[string]any{"section": "thermal", "action": "boost"}); err == nil {
		t.Fatal("competing controller accepted")
	}
	if calls != 0 {
		t.Fatal("started a child for a rejected write")
	}
	if _, err := c.Call(map[string]any{"section": "thermal", "action": "status"}); err != nil {
		t.Fatal(err)
	}
	if calls != 1 {
		t.Fatal(calls)
	}
}

func TestBusyGateRejectsRatherThanQueuingUnboundedHardwareWork(t *testing.T) {
	gate.Lock()
	defer gate.Unlock()
	c := &Client{Run: func(context.Context, []byte) ([]byte, error) { t.Fatal("child started while busy"); return nil, nil }}
	if _, err := c.Call(map[string]any{"action": "status"}); err == nil {
		t.Fatal("busy hardware accepted")
	}
}

func TestRequestLimitBeforeChildStarts(t *testing.T) {
	c := &Client{Run: func(context.Context, []byte) ([]byte, error) {
		t.Fatal("oversized request reached child")
		return nil, nil
	}}
	if _, err := c.Call(map[string]any{"colors": strings.Repeat("a", 65537)}); err == nil {
		t.Fatal("oversized request accepted")
	}
}

func TestOutputCeiling(t *testing.T) {
	var b limitedBuffer
	if _, err := b.Write(make([]byte, Limit)); err != nil {
		t.Fatal(err)
	}
	if _, err := b.Write([]byte("x")); err == nil {
		t.Fatal("output limit ignored")
	}
}

func TestNativeBusyRetainsRetryableCode(t *testing.T) {
	c := &Client{Run: func(context.Context, []byte) ([]byte, error) {
		return []byte(`{"ok":false,"code":"device-busy"}`), fmt.Errorf("exit 1")
	}}
	if _, err := c.Call(map[string]any{"action": "status"}); !errors.Is(err, ErrBusy) {
		t.Fatalf("lost busy error: %v", err)
	}
}
