import asyncio

import toga
from toga import Button, MultilineTextInput
from toga.style import Pack
from toga.style.pack import COLUMN

import bluetooth
from bluetooth import *
import select 

class ConnectionWindow():
    def  ConnectionWindow_Startup(self):
        """Set up the GUI."""
        self.SpecificScan_button = Button(
            'Scan for Bluetooth Devices',
            on_press=self.start_scan,
        )

        self.device_list = MultilineTextInput(
            readonly=True,
            style=Pack(padding=(10, 5), height=200),
        )
        self.data_list = MultilineTextInput(
            readonly=True,
            style=Pack(padding=(10, 5), height=200),
        )
        box = toga.Box(
            children=[
                self.SpecificScan_button,
                self.device_list,
                self.data_list,
            ],
            style=Pack(direction=COLUMN),
        )

        self.main_window = toga.MainWindow(
            title='Bluetooth SCAN'
        )
        self.main_window.content = box
        self.main_window.show()
        self.scan_on = False

    async def start_scan(self, widget):  
        self.device_list.value = ("Performing inquiry...\n")
        nearby_devices = bluetooth.discover_devices(duration=8, lookup_names=True, flush_cache=True, lookup_class=False)
        
        self.device_list.value=("Found {} devices\n".format(len(nearby_devices)))

        for addr, name in nearby_devices:
            try:
                self.data_list.value =("   {} - {}".format(addr, name))
            except UnicodeEncodeError:
                self.data_list.value =("   {} - {}".format(addr, name.encode("utf-8", "replace")))
        self.show_scan_result(nearby_devices)


    def show_scan_result(self, data):
        #Show names of found devices and attached advertisment data.
        self.print_data('Device data:\n', clear=True)
        for addr, name in data:
            self.print_device(name)
            self.print_data(addr)

    def print_device(self, device='', clear=False):
        #Write device name to MultilineTextInput for devices.
        if clear:
            self.device_list.value = ''
        self.device_list.value += device + '\n'
        self.device_list.scroll_to_bottom()

    def print_data(self, data='', clear=False):
        # Write device data to MultilineTextInput for device data.
        if clear:
            self.data_list.value = ''
        self.data_list.value += data + '\n'
        self.data_list.scroll_to_bottom()
