Xenos
=====

Windows dll injector. Based on Blackbone library - https://github.com/DarthTon/Blackbone

## Features ##

- Supports x86 and x64 processes and modules
- Kernel-mode injection feature (driver required)
- Manual map of kernel drivers (driver required)
- Injection of pure managed images without proxy dll
- Windows 7 cross-session and cross-desktop injection
- Injection into native processes (those having only ntdll loaded)
- Calling custom initialization routine after injection
- Unlinking module after injection
- Injection using thread hijacking
- Injection of x64 images into WOW64 process
- Image manual mapping
- Injection profiles

Manual map features:
- Relocations, import, delayed import, bound import
- Hiding allocated image memory (driver required)
- Static TLS and TLS callbacks
- Security cookie
- Image manifests and SxS
- Make module visible to GetModuleHandle, GetProcAddress, etc.
- Support for exceptions in private memory under DEP
- C++/CLI images are supported (use 'Add loader reference' in this case)

Supported OS: Win7 - Win10 x64

## License ##
Xenos is licensed under the MIT License. Dependencies are under their respective licenses.

[![Build status](https://ci.appveyor.com/api/projects/status/eu6lpbla89gjgy5m?svg=true)](https://ci.appveyor.com/project/DarthTon/xenos)