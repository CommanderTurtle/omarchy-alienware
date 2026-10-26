package backend

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"sync"
	"time"
)

const Binary = "/usr/lib/omarchy-alienware/alienware-backend"
const Limit = 256 * 1024

var gate sync.Mutex
var ErrBusy = errors.New("hardware backend is busy")

type Client struct {
	Run      func(context.Context, []byte) ([]byte, error)
	Conflict func() bool
}

func Installed() bool {
	info, err := os.Stat(Binary)
	return err == nil && info.Mode().IsRegular() && info.Mode().Perm()&0022 == 0
}

type limitedBuffer struct{ bytes.Buffer }

func (b *limitedBuffer) Write(p []byte) (int, error) {
	if len(p) > Limit-b.Len() {
		return 0, errors.New("backend output exceeded limit")
	}
	return b.Buffer.Write(p)
}

func (c *Client) Call(request map[string]any) ([]byte, error) {
	raw, err := json.Marshal(request)
	if err != nil || len(raw) > 65536 {
		return nil, fmt.Errorf("backend request exceeds limit")
	}
	if !gate.TryLock() {
		return nil, ErrBusy
	}
	defer gate.Unlock()
	if request["section"] == "thermal" && request["action"] != "status" && c.Conflict != nil && c.Conflict() {
		return nil, fmt.Errorf("another thermal controller is active; stop awccd/fancontrol before writing from this plugin")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 4*time.Second)
	defer cancel()
	var out []byte
	if c.Run != nil {
		out, err = c.Run(ctx, raw)
	} else {
		cmd := exec.CommandContext(ctx, Binary)
		cmd.Env = []string{"PATH=/usr/bin", "HOME=/", "LC_ALL=C"}
		cmd.Stdin = bytes.NewReader(raw)
		cmd.WaitDelay = 250 * time.Millisecond
		var stdout, stderr limitedBuffer
		cmd.Stdout, cmd.Stderr = &stdout, &stderr
		err = cmd.Run()
		out = stdout.Bytes()
	}
	if ctx.Err() != nil {
		return nil, fmt.Errorf("hardware backend timed out; no automatic retry was sent")
	}
	if len(out) > Limit {
		return nil, fmt.Errorf("backend output exceeded limit")
	}
	var reply struct {
		OK    bool   `json:"ok"`
		Code  string `json:"code"`
		Error string `json:"error"`
	}
	if parseErr := json.Unmarshal(out, &reply); parseErr != nil {
		return nil, fmt.Errorf("invalid backend response: %w", parseErr)
	}
	if !reply.OK {
		if reply.Code == "device-busy" {
			return nil, ErrBusy
		}
		return nil, fmt.Errorf("backend: %s", reply.Error)
	}
	if err != nil {
		return nil, fmt.Errorf("backend failed: %w", err)
	}
	return out, nil
}
