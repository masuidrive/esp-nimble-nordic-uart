PROJECT_NAME := main


example:
	idf.py build flash monitor -p /dev/cu.usbserial-*

test:
	cd test && idf.py build flash monitor -p /dev/cu.usbserial-*

include $(IDF_PATH)/make/project.mk
