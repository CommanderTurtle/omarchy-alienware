const assert = require("node:assert/strict")
const test = require("node:test")
const fs = require("node:fs")
const path = require("node:path")
const { Model } = require("./load.js")

test("unknown firmware capabilities never offer guessed profiles or curves", () => {
  const status = Model.normalizeStatus({ backend: { name: "awcc", curveSupported: false }, profile: { choices: [] } })
  assert.deepEqual(Model.profileChoices(status), [])
  assert.equal(status.backend.curveSupported, false)
})

test("missing RPM is not an idle fan", () => {
  const status = Model.normalizeStatus({ fans: [{ id: "cpu", readable: false }] })
  assert.equal(status.fans[0].stopped, false)
  assert.equal(status.fans[0].readable, false)
  assert.equal(Model.fansStopped(status), false)
  assert.ok(Number.isNaN(Model.maxRpm(status)))
  assert.equal(Model.barView(status, "ok", ["fan"], 90).segments[0].text, "-")
})

test("busy status keeps the last readings stale rather than reporting a missing controller", () => {
  assert.equal(Model.statusHealth(true, { ok: false, code: "device-busy" }, false), "stale")
})

test("generic device colors persist without inventing lighting state on fresh installs", () => {
  assert.equal(Model.hasLightingState(Model.parseState("")), false)
  assert.equal(Model.hasLightingState(Model.parseState(JSON.stringify({ curve: { interval: 2 } }))), false)
  const payload = Model.buildStatePayload({ devices: { "187c:0551": "#f00", "not-a-device": "00FF00", "0d62:0100": "oops" } })
  const back = Model.parseState(JSON.stringify(payload))
  assert.deepEqual(back.devices, { "187c:0551": "FF0000" })
  assert.equal(Model.hasLightingState(back), true)
})

test("legacy chassis UI requires every verified region", () => {
  assert.equal(Model.hasLegacyChassis([{ regions: { keyboard: [1, 2] } }]), false)
  assert.equal(Model.hasLegacyChassis([{ mapped: false, regions: { power: [0], logo: [1], "ring-top": [8], "ring-bottom": [16] } }]), false)
  assert.equal(Model.hasLegacyChassis([{ mapped: true, regions: { power: [0], logo: [1], "ring-top": [8], "ring-bottom": [16] } }]), true)
})

test("native positional keyboard gate follows the measured UI wire-index set", () => {
  const contract = fs.readFileSync(path.join(__dirname, "../backend/contracts.h"), "utf8")
  const indices = contract.match(/x15KeyIndices\[\] = \{([\s\S]*?)\}/)[1].match(/\d+/g).map(Number)
  const measured = [...new Set(Model.keyboardPaintableIds().flatMap(id => Model.keyIndices(Model.keyboardKeyById(id))))].sort((a, b) => a - b)
  assert.deepEqual(indices, measured)
})

test("database and backend source pins are explicit, not floating branches", () => {
  const root = path.join(__dirname, "..")
  const lock = JSON.parse(fs.readFileSync(path.join(root, "upstream.lock.json"), "utf8"))
  for (const dep of Object.values(lock)) assert.match(dep.revision, /^[a-f0-9]{40}$/)
  const database = JSON.parse(fs.readFileSync(path.join(root, "database.json"), "utf8"))
  assert.ok(Object.keys(database).length > 50)
  for (const value of Object.values(database)) {
    if (value.featureSet !== undefined) assert.match(value.featureSet, /^[01]{7}$/)
    assert.match(value.thermalModes, /^[01]{8}$/)
  }
  assert.equal(database["Alienware m15 Ryzen Ed. R5"].featureSet, undefined)
})

test("normal install cannot pull the old upstream prebuilt helper", () => {
  const root = path.join(__dirname, "..")
  const install = fs.readFileSync(path.join(root, "scripts/install-helper.sh"), "utf8")
  assert.doesNotMatch(install, /packaging\/bin/)
  const pkg = fs.readFileSync(path.join(root, "packaging/PKGBUILD"), "utf8")
  assert.match(pkg, /git\+file:/)
  assert.doesNotMatch(pkg, /packaging\/udev/)
  const service = fs.readFileSync(path.join(root, "packaging/systemd/alienwarectl.service"), "utf8")
  assert.doesNotMatch(service, /^Conflicts=/m)
  assert.match(service, /ExecStartPre=.*check-controllers/)
})
