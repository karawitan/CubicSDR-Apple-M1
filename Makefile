default:
	./build-osx.sh
clean:
	rm -rf build

test:
	sudo build/cjcliffe/CubicSDR-build/x64/CubicSDR


# on m1, SDRplay_RSP_API-MacOS-3.07.a4.run from www.sdrplay.com does not work as prefix is /usr and system protection prevents any write to this path

# on m1, requires SDRplay_RSP_API-MacOSX-3.07.3.pkg (download for Catalina, not m1). Target prefix is /usr/local, which is allowed by system protection

driver.unload: sdrplay-api-driver.unload

driver.load: sdrplay-api-driver.load

# boggus grep due to 
sdrplay-api-driver.unload:
	ps -eaf | grep -v grep | grep -q sdrplay_apiService && sudo launchctl unload -w /Library/LaunchDaemons/com.sdrplay.sdrplay_service.plist || true
	ps -eaf | grep -v grep | grep -q sdrplay_apiService && sudo launchctl unload -w /Library/LaunchAgents/com.sdrplay.sdrplay_service.plist || true
	ps -eaf | grep -v grep | grep -q sdrplay_apiService && exit 1 || true

sdrplay-api-driver.load:
	#sudo launchctl load -w /Library/LaunchDaemons/com.sdrplay.sdrplay_service.plist

api-driver.foreground: sdrplay-api-driver.unload
	sudo /usr/local/bin/sdrplay_apiService -Module Full
