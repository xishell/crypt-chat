project('crypt-chat', 'c',
  default_options: [
    'warning_level=1',
    'c_std=gnu17',
  ],
)

cc = meson.get_compiler('c')

inc = include_directories('include')

sources = files(
  'src/aead.c',
  'src/boot.S',
  'src/chacha20.c',
  'src/chat_framer.c',
  'src/chat_protocol.c',
  'src/devices.c',
  'src/dtekv-lib.c',
  'src/main.c',
  'src/poly1305.c',
  'src/uart.c',
  'src/utils.c',
)

softfloat = cc.find_library('softfloat',
  dirs: meson.project_source_root(),
  static: true,
)

linker_script = meson.project_source_root() / 'dtekv-script.lds'

elf = executable('crypt-chat',
  sources,
  include_directories: inc,
  dependencies: softfloat,
  link_args: ['-T', linker_script, '-nostdlib'],
  name_suffix: 'elf',
)

objcopy = find_program('riscv32-unknown-elf-objcopy')

bin = custom_target('crypt-chat.bin',
  input: elf,
  output: 'crypt-chat.bin',
  command: [objcopy, '--output-target', 'binary', '@INPUT@', '@OUTPUT@'],
  build_by_default: true,
)

run_target('upload',
  command: ['bash', meson.project_source_root() / 'run_lab.sh'],
)
