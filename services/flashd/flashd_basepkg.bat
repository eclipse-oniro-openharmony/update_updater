setlocal enabledelayedexpansion
@echo -----------------------------
@echo %date%-%time%
@echo "Baltimore Updating:"
@echo "Start Reboot Flashd..."
hdc shell write_updater boot_flash
hdc shell reboot updater
@C:\Windows\System32\PING.EXE -n 15 127.0.0.1>nul

@SET images_table=^
    flash:uboot:uboot.img ^
    flash:updater:updater.img ^
	flash:boot_linux:boot_linux.img ^
	flash:ramdisk:ramdisk.img ^
	flash:system:system.img ^
	flash:vendor:vendor.img ^
    flash:resource:resource.img

@echo off
@FOR %%i IN (%images_table%) do (
    @for /f "delims=: tokens=1,2,3" %%j in ("%%i") do (
            @if "%%j"=="flash" (
				@echo start flash %%k
				set start=!time!
				hdc flash %%k  -f %~dp0%%l  2>&1 |find "[Success]"
				@if errorlevel 1 (
					@echo flash %%k fail, please check
					goto error
				)
				set end=!time!
				call:GetTimes !start! !end!
            )
    )
	C:\Windows\System32\PING.EXE -n 3 127.0.0.1>nul
)
@hdc shell ./bin/updater_reboot >nul
@goto sucess


:error
@echo "Update Failed!"
@pause
@goto end

:sucess
@echo updater success
@pause
@goto end

:GetTimes
@echo off
set options="tokens=1-4 delims=:.,"
for /f %options% %%a in ("%~1") do set start_h=%%a&set /a start_m=100%%b %% 100&set /a start_s=100%%c %% 100&set /a start_ms=100%%d %% 100
for /f %options% %%a in ("%~2") do set end_h=%%a&set /a end_m=100%%b %% 100&set /a end_s=100%%c %% 100&set /a end_ms=100%%d %% 100
set /a hours=%end_h%-%start_h%
set /a mins=%end_m%-%start_m%
set /a secs=%end_s%-%start_s%
set /a ms=%end_ms%-%start_ms%
if %ms% lss 0 set /a secs = %secs% - 1 & set /a ms = 100%ms%
if %secs% lss 0 set /a mins = %mins% - 1 & set /a secs = 60%secs%
if %mins% lss 0 set /a hours = %hours% - 1 & set /a mins = 60%mins%
if %hours% lss 0 set /a hours = 24%hours%
if 1%ms% lss 100 set ms=0%ms%
:: 计算时间并输出
set /a totalsecs = %hours%*3600 + %mins%*60 + %secs%
echo Total time %totalsecs%.%ms%s 
echo --------------------------------------------------


:end
