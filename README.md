Xenos
=====

Windows dll injector. This project completely depends on Blackbone library - https://github.com/DarthTon/Blackbone

## Features ##

- Supports x86 and x64 processes and modules
- Injection of pure managed images without proxy dll
- Windows 7 cross-session and cross-desktop injection
- Injection into native processes (those having only ntdll loaded)
- Calling custom initialization routine after injection
- Unlinking module after injection
- Injection using thread hijacking
- Injection of x64 images into WOW64 process(read more in Additional notes section)
- Image manual mapping

Supported OS: Win7 - Win8.1 x64

## License ##
Xenos is licensed under the MIT License. Dependencies are under their respective licenses.