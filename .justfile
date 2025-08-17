set windows-shell := ['pwsh', '-Command']

deploy:
	C:\Qt\6.9.1\llvm-mingw_64\bin\windeployqt.exe --no-quick-import --no-translations \
	    --no-system-d3d-compiler --no-virtualkeyboard --no-compiler-runtime --no-opengl-sw \
	    build\Desktop_Qt_6_9_1_llvm_mingw_64_bit-Release\Evanesco.exe
	cp C:\Qt\6.9.1\llvm-mingw_64\bin\libunwind.dll build\Desktop_Qt_6_9_1_llvm_mingw_64_bit-Release
	cp C:\Qt\6.9.1\llvm-mingw_64\bin\libc++.dll build\Desktop_Qt_6_9_1_llvm_mingw_64_bit-Release
