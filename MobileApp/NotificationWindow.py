
import toga
from toga.style import Pack
from toga.style.pack import COLUMN, LEFT
import socket


class NotificationWindow():

    def NotificationWindow_Startup(self):

        self.notification_label = toga.Label("No Notes", style=Pack(text_align=LEFT))
        
        self.notification_box = toga.Box(
            style=Pack(direction=COLUMN),
            children=[
                self.notification_label
                ],
            )
        self.notification_window = toga.MainWindow()        
        self.notification_window.content= self.notification_box
        self.notification_window.show()