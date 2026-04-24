# Build Commands (v2)

Run from any PowerShell terminal:

```powershell
& 'E:\Visual Studio IDE\MSBuild\Current\Bin\MSBuild.exe' 'D:\Desktop\Fmod Pjkt\New Wwise\ArpweaverFm_v2\ArpweaverFm_Windows_vc170_static.sln' /t:Build /p:Configuration=Release /p:Platform=x64 /m /verbosity:minimal
& 'E:\Visual Studio IDE\MSBuild\Current\Bin\MSBuild.exe' 'D:\Desktop\Fmod Pjkt\New Wwise\ArpweaverFm_v2\ArpweaverFm_Authoring_Windows_vc170.sln' /t:Build /p:Configuration=Release /p:Platform=x64 /m /verbosity:minimal
```

Expected outputs:
- Runtime static lib:
  - `E:\Wwise2025.1.6.9117\SDK\x64_vc170\Release\lib\ArpweaverFmSource.lib`
- Authoring plugin DLL/XML:
  - `E:\Wwise2025.1.6.9117\Authoring\x64\Release\bin\Plugins\ArpweaverFm.dll`
  - `E:\Wwise2025.1.6.9117\Authoring\x64\Release\bin\Plugins\ArpweaverFm.xml`
