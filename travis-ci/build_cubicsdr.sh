# CubicSDR
cd $HOME/build
mkdir -p cjcliffe/CubicSDR-build
cd cjcliffe/CubicSDR-build
cmake ../../../ -DCMAKE_BUILD_TYPE=Release \
	-DUSE_HAMLIB=1 \
	-DENABLE_DIGITAL_LAB=1 \
	-DwxWidgets_CONFIG_EXECUTABLE=/opt/homebrew/bin/wx-config \
	-DCXX_INCLUDES="$(/opt/homebrew/bin/wx-config --cxxflags)"
	#-DCXX_=$(/opt/homebrew/bin/wx-config --libs)
	#-DwxWidgets_CONFIG_EXECUTABLE=
	#-DwxWidgets_CONFIG_EXECUTABLE=$HOME/build/wxWidgets/staticlib/bin/wx-config \
make 

	#-DwxWidgets_LIBRAIRIES=/opt/homebrew/Cellar/wxwidgets/3.2.2.1/lib \
	#-DwxWidgets_INCLUDE_DIRS=/opt/homebrew/Cellar/wxwidgets/3.2.2.1/include \

