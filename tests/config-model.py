import json
import pathlib
import stat
import subprocess
import tempfile

with tempfile.TemporaryDirectory(dir='build-host') as folder:
    path = pathlib.Path(folder) / 'config.json'
    original = {'api_key': 'fixture-only', 'model': 'old-model', 'timezone': 'America/New_York', 'demo_mode': False, 'custom': {'keep': True}}
    path.write_text(json.dumps(original))
    def run(model):
        return subprocess.run(['build-host/config-model', model, str(path)], capture_output=True)
    assert run('claude-opus-5-5').returncode == 0
    assert json.loads(path.read_text()) == dict(original, model='claude-opus-5-5')
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    previous = path.read_bytes()
    assert run('bad model').returncode != 0 and path.read_bytes() == previous
    for invalid in ['{bad json', '{}']:
        path.write_text(invalid)
        assert run('claude-opus-5-5').returncode != 0 and path.read_text() == invalid

    # --set KEY reads the value from stdin (keeps secrets out of argv); only known keys are accepted.
    path.write_text(json.dumps(original))
    def setkey(key,value):
        return subprocess.run(['build-host/config-model','--set',key,str(path)],input=value.encode(),capture_output=True)
    assert setkey('gmail_address','me@example.com').returncode == 0
    assert setkey('gmail_app_password','abcdabcdabcdabcd\n').returncode == 0
    saved = json.loads(path.read_text())
    assert saved['gmail_address'] == 'me@example.com' and saved['gmail_app_password'] == 'abcdabcdabcdabcd'
    assert saved['api_key'] == 'fixture-only' and saved['custom'] == {'keep': True}
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    before = path.read_bytes()
    assert setkey('api_endpoint','http://evil').returncode != 0 and path.read_bytes() == before
    assert setkey('name','').returncode != 0 and path.read_bytes() == before
    assert b'abcdabcdabcdabcd' not in setkey('gmail_app_password','abcdabcdabcdabcd').stdout
print('PASS: model updates preserve key/settings, owner-only permissions, invalid input leaves config unchanged, --set from stdin for known keys')
