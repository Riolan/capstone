
from smarttrailcam.ConnectionWindow import *
from smarttrailcam.NotificationWindow import *
from smarttrailcam.StoredImagesWindow import *
from smarttrailcam.ImageFeedWindow import *

from threading import *

import toga 
from toga import Button, MultilineTextInput
from toga.style.pack import *

if toga.platform.current_platform == 'android':
    from .bleekWare.Scanner import Scanner as Scanner
else:
    from bleak import BleakScanner as Scanner

class SmartTrailCam(toga.App):
    def startup(self):
        
        self.main_window = toga.MainWindow()
        
#************************************** Declaration of Windows *************************************************        
        
        self.stored_image_window = toga.Window()
        self.notification_window = toga.Window()
        self.image_feed_window = toga.Window()
        self.pairing_connection_window = toga.Window()
        
	
#************************************** Declaration of Buttons *************************************************  
        button_style = Pack(flex=1)
        
        Stored_image_button = toga.Button(
            "Stored Images", 
            on_press=self.Stored_Images, 
            style=button_style)
            
        Notification_button = toga.Button(
            "Notifications", 
            on_press=self.Notifications, 
            style=button_style)
            
        Image_feed_button = toga.Button(
            "Image Feed", 
            on_press=self.Image_Feed, 
            style=button_style)
            
        Pairing_connection_button = toga.Button(
            "Connections", 
            on_press=self.Pairing_Connection, 
            style=button_style)
          

#************************************** Declaration of Components ************************************************* 
        
  #Box to hold notifications 

#************************************** Declaration of Boxes *************************************************        
        
        main_box = toga.Box(
            style=Pack(direction=COLUMN),
            children=[  
                Stored_image_button ,
                Notification_button ,
                Image_feed_button,
                Pairing_connection_button,
            ],
        )
        
        
        
#************************************** Display main window *************************************************  
        self.main_window.content = main_box
        self.main_window.show()

#************************************** Function Definitions *************************************************  
    
    def Stored_Images(self, widget):
        StoredImages = StoredImagesWindow()
        StoredImages.StoredImagesWindow_Startup()
        
    def Notifications(self, widget):
        Notifications = NotificationWindow()
        Notifications.NotificationWindow_Startup()
    	
    def Image_Feed(self, widget):
        Feed = ImageFeedWindow()
        Feed.ImageFeedWindow_Startup()
    	
                    
     	
    def Pairing_Connection(self, widget):
        Scanner = ConnectionWindow()
        Scanner.ConnectionWindow_Startup()
        

#*************************************************************************************************  
    
 
def main():
    return SmartTrailCam()
