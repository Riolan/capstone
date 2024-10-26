import asyncio

import toga
from toga import Button, MultilineTextInput
from toga.style import Pack
from toga.style.pack import COLUMN

if toga.platform.current_platform == 'android':
    from .bleekWare.Scanner import Scanner as Scanner
else:
    from bleak import BleakScanner as Scanner
    from bleak import BleakClient 

class ConnectionWindow():
    def  ConnectionWindow_Startup(self):
        """Set up the GUI."""
        self.SpecificScan_button = Button(
            'Scan for BLE devices using Specific Address',
            on_press=self.start_scan,
        )
        
        self.GeneralScan_button = Button(
            'Scan for all BLE devices',
            on_press=self.start_discover,
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
                self.GeneralScan_button,
                self.device_list,
                self.data_list,
            ],
            style=Pack(direction=COLUMN),
        )

        self.main_window = toga.MainWindow(
            title='BLE SCAN'
        )
        self.main_window.content = box
        self.main_window.show()
        self.scan_on = False

    async def start_scan(self, widget):
        devices = await Scanner.discover()
        for device in devices:
            print(device)
                
        # used my phone as an example lol
        address = "08:87:C7:DC:5F:EC" 
        async with BleakClient(address) as client:
            if client.is_connected:
                print("Connected to phone!")
                # Perform operations with the connected phone here

    async def start_discover(self, widget):
        self.print_device('Start BLE scan...', clear=True)
        self.print_data(clear=True)

        result = await Scanner.discover(return_adv=True)
        self.print_device('...scanning stopped.')
        self.show_scan_result(result)


    def show_scan_result(self, data):
        #Show names of found devices and attached advertisment data.
        self.print_device('Found devices:')
        self.print_data('Device data:', clear=True)
        for key in data:
            device, adv_data = data[key]
            self.print_device(self.get_name(device))
            self.print_data(f'{device}\n{adv_data}')
            self.print_data()

    def get_name(self, device):
        #Return name or address of BLE device.
        if device.name:
            return device.name
        else:
            return f'No name ({device.address})'

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
