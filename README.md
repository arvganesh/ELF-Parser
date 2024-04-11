# ELF-Parser

Hello World Program Header Types:
PT_LOAD
PT_LOAD
PT_LOAD
PT_LOAD
PT_NOTE
PT_NOTE
PT_TLS
PT_GNU_PROPERTY
PT_GNU_STACK
PT_GNU_RELRO

process 82016
Mapped address spaces:

          Start Addr           End Addr       Size     Offset  Perms  objfile
            0x700000           0x701000     0x1000        0x0  r--p   /users/arvgan/ELF-Parser/helloworld_static
            0x701000           0x798000    0x97000     0x1000  r-xp   /users/arvgan/ELF-Parser/helloworld_static
            0x798000           0x7c1000    0x29000    0x98000  r--p   /users/arvgan/ELF-Parser/helloworld_static
            0x7c1000           0x7c5000     0x4000    0xc0000  r--p   /users/arvgan/ELF-Parser/helloworld_static
            0x7c5000           0x7c8000     0x3000    0xc4000  rw-p   /users/arvgan/ELF-Parser/helloworld_static
            0x7c8000           0x7ef000    0x27000        0x0  rw-p   [heap]
      0x7ffff7ff9000     0x7ffff7ffd000     0x4000        0x0  r--p   [vvar]
      0x7ffff7ffd000     0x7ffff7fff000     0x2000        0x0  r-xp   [vdso]
      0x7ffffffde000     0x7ffffffff000    0x21000        0x0  rw-p   [stack]
  0xffffffffff600000 0xffffffffff601000     0x1000        0x0  --xp   [vsyscall]