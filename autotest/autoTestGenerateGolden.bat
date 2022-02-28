del /f /q .\*.png

cd ..\bin
.\RelWithDebInfo\tutorial_raytracing.exe -v --listgpus --autoTest --model sponza

cd ..\bin
.\RelWithDebInfo\tutorial_raytracing.exe -v --listgpus --autoTest --model cornell

cd ..\bin
.\RelWithDebInfo\tutorial_raytracing.exe -v --listgpus --autoTest --model sphere

cd ..\bin
.\RelWithDebInfo\tutorial_raytracing.exe -v --listgpus --autoTest --model venus





