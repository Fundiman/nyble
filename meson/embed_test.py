import importlib.util
import os
import tempfile
import types

here = os.path.dirname(os.path.realpath(__file__))
spec = importlib.util.spec_from_file_location('ant_embed', os.path.join(here, 'embed.py'))
embed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embed)

with tempfile.TemporaryDirectory() as tmp:
  include = os.path.join(tmp, 'include')
  os.mkdir(include)

  header = os.path.join(include, 'ant.h')
  sdk_settings = os.path.join(tmp, 'SDKSettings.json')
  with open(header, 'w', encoding='utf-8'):
    pass
  with open(sdk_settings, 'w', encoding='utf-8'):
    pass

  output = f'ant.o: {header} {sdk_settings}\n'
  original_run = embed.subprocess.run
  embed.subprocess.run = lambda *args, **kwargs: types.SimpleNamespace(stdout=output)
  try:
    deps = embed.header_closure(
      tmp,
      {'compiler': ['cc'], 'parameters': ['-Iinclude']},
      [header],
    )
  finally:
    embed.subprocess.run = original_run

  assert os.path.realpath(header) in deps, deps
  assert os.path.realpath(sdk_settings) not in deps, deps

print('embed-header-closure:ok')
