package backend

import (
	"context"
	"errors"
	"time"

	"github.com/godbus/dbus/v5"
)

func SystemdConflict(conn *dbus.Conn) func() bool {
	return func() bool {
		check, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		manager := conn.Object("org.freedesktop.systemd1", "/org/freedesktop/systemd1")
		for _, unit := range []string{"awccd.service", "fancontrol.service", "i8kmon.service", "tuxedo-control-center.service"} {
			var path dbus.ObjectPath
			err := manager.CallWithContext(check, "org.freedesktop.systemd1.Manager.GetUnit", 0, unit).Store(&path)
			if err != nil {
				var dbusErr dbus.Error
				if errors.As(err, &dbusErr) && dbusErr.Name == "org.freedesktop.systemd1.NoSuchUnit" {
					continue
				}
				return true
			}
			var state dbus.Variant
			err = conn.Object("org.freedesktop.systemd1", path).CallWithContext(check, "org.freedesktop.DBus.Properties.Get", 0,
				"org.freedesktop.systemd1.Unit", "ActiveState").Store(&state)
			if err != nil || (state.Value() != "inactive" && state.Value() != "failed") {
				return true
			}
		}
		return false
	}
}
