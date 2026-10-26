package hw

import (
	"encoding/json"
	"fmt"
)

type BackendInfo struct {
	Name           string `json:"name"`
	SupportedModel bool   `json:"supportedModel"`
	CurveSupported bool   `json:"curveSupported"`
}

type thermalReply struct {
	Backend        string         `json:"backend"`
	SupportedModel bool           `json:"supportedModel"`
	CurveSupported bool           `json:"curveSupported"`
	Fans           []Fan          `json:"fans"`
	Temps          map[string]int `json:"temps"`
	Profile        Profile        `json:"profile"`
	Warnings       []string       `json:"warnings"`
}

func (r *Reader) backendStatus() (thermalReply, error) {
	var result thermalReply
	raw, err := r.Backend.Call(map[string]any{"section": "thermal", "action": "status"})
	if err != nil {
		return result, err
	}
	err = json.Unmarshal(raw, &result)
	return result, err
}

func (r *Reader) backendWrite(action string, values map[string]any) error {
	values["section"], values["action"] = "thermal", action
	_, err := r.Backend.Call(values)
	if err != nil {
		return fmt.Errorf("%w: %w", ErrNotSupported, err)
	}
	return nil
}

func (r *Reader) CanCurve() bool {
	if r.Backend == nil {
		return true
	}
	status, err := r.backendStatus()
	return err == nil && status.CurveSupported && (r.PPDRunning == nil || !r.PPDRunning())
}
