@echo off
REM 啟動 SkillCreatorUE5 Standalone 並自動存 log
REM Log 固定輸出至：C:\SkillCreatorUE5\GameLogs\latest.log
REM （CLAUDE 讀 log 的路徑就是這裡，不需要加 -log 手動執行）

set EXE="C:\SkillCreatorUE5\Packaged\Windows\SkillCreatorUE5.exe"
set LOGFILE="C:\SkillCreatorUE5\GameLogs\latest.log"

if not exist %EXE% (
    echo [ERROR] 找不到執行檔：%EXE%
    echo 請先打包（RunUAT BuildCookRun）
    pause
    exit /b 1
)

echo [RunGame] 啟動中... Log 輸出至 %LOGFILE%
%EXE% -log -ABSLOG=%LOGFILE%
