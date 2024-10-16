import io
import asyncio
import socket
import sys
import time

from threading import *
from PIL import Image, ImageDraw

import toga
from toga.style.pack import *


class ImageViewApp(toga.App):
    def startup(self):
        
        self.main_window = toga.MainWindow()
        
#************************************** Declaration of Windows *************************************************        
        
        self.stored_image_window = toga.Window()
        self.notification_window = toga.Window()
        self.image_feed_window = toga.Window()
        self.pairing_connection_window = toga.Window()
        
	
#************************************** Declaration of Buttons *************************************************  
        button_style = Pack(flex=1)
        
        self.stored_image_window.next_button = toga.Button( 
            "Next Image", 
            on_press=self.Next_Picture, 
            style=button_style)
            
       
        self.stored_image_window.prev_button = toga.Button(
             "Previous Image",
             enabled=False,
             on_press=self.Prev_Picture,
             style=button_style)

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
                  
  #count to iterate through images
        global image_count
        image_count = 0        
               
  #Image view to hold image in stored image window
        self.stored_image_window_image_view = toga.ImageView(
                    style=Pack(flex=1, width=300, alignment=CENTER)
                    )
        image_being_viewed = ("animalPics/0.jpg")
        self.stored_image_window_image_view.image = image_being_viewed
        
  #Box to hold notifications 
        self.notification_label = toga.Label("No Notes", style=Pack(text_align=LEFT))
        
#************************************** Declaration of Boxes *************************************************        

      
        stored_image_window_inner_box = toga.Box(
        style=Pack(direction=ROW),
            children=[
                self.stored_image_window.next_button,
                self.stored_image_window.prev_button,
                ],
        )
              
               
        self.stored_image_window_outer_box = toga.Box(
            style=Pack(direction=COLUMN, flex=1),
            children=[
                stored_image_window_inner_box,
                self.stored_image_window_image_view, 
                toga.Label(text="Animal Picture", style=Pack(text_align=CENTER)),
            ],
        )
        
     
        
        self.notification_box = toga.Box(
            style=Pack(direction=COLUMN),
            children=[
                self.notification_label
                ],
            )
                
        
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
        
    def Next_Picture(self, button):
        global image_count
        image_count += 1  
        self.stored_image_window.prev_button.enabled = True
        image_being_viewed = ("animalPics/" + str(image_count)+ ".jpg")
        if (image_count > 3):
        	stored_image_window.next_button.enabled = False
        	stored_image_window.prev_button.enabled = True
        
        self.stored_image_window_image_view.image = image_being_viewed
      
        
    def Prev_Picture(self, button):
        global image_count
        image_count -= 1
        if (image_count < 1):
        	self.stored_image_window.prev_button.ennabled = False
        	self.stored_image_window.next_button.enabled = True
        image_being_viewed = "animalPics/" + str(image_count)+ ".jpg"
        self.stored_image_window_image_view.image = image_being_viewed
  
        
    def Stored_Images(self, widget, **kwargs):
        self.stored_image_window.content = self.stored_image_window_outer_box
        self.stored_image_window.show()
      
    
    def Notifications(self, widget, **kwargs):
        notification_window.content= self.notification_box
        self.notificaiton_window.show()
    	
    def Image_Feed(self, widget, **kwargs):
     	self.image_feed_window.show()
     	
    def ExampleSocket(self):
        # get the hostname
        host = socket.gethostname()
        port = 5000  # initiate port no above 1024

        global server_socket 
        server_socket = socket.socket()  # get instance
    	# look closely. The bind() function takes tuple as argument
        server_socket.bind((host, port))  # bind host address and port together

    	# configure how many client the server can listen simultaneously
        server_socket.listen(2)
        conn, address = server_socket.accept()  # accept new connection
        print("Connection from: " + str(address))
        while True:
        # receive data stream. it won't accept data packet greater than 1024 bytes
            data = conn.recv(1024).decode()
            if not data:
            # if data is not received break
                break
            print("from connected user: " + str(data))
            self.notification_box = str(data)
            data = input(' -> ')
            conn.send(data.encode())  # send data to the client

        conn.close()  # close the connection
                    
     	
    def Pairing_Connection(self, widget, **kwargs):
        pairing_connection_window = toga.Window(
            "Connections Window",
            )    
        self.pairing_connection_window.content = self.notification_box
        self.pairing_connection_window.show()
        if (self.pairing_connection_window.visible == True):
            self.Socket()
        
        
    def Socket(self):
        s = socket.socket()
        host = socket.gethostname()
        port = 1247
        s.bind((host,port))
        s.listen(5)
        while True:
            c, addr = s.accept()
            print("Connection accepted from " + repr(addr[1]))

            c.send("Server approved connection\n")
            print (repr(addr[1]) + ": " + c.recv(1026))
            c.close()
        while True:
            rcvdData = c.recv(1024).decode()
            pairing_connection_window.content = notification_box
            pairing_connection_window.show()
            notification_label.text = rcvdData
            pairing_connection_window.content= notification_box
            if (pairing_connection_window.closed == True):
                break
            
        c.close()
        s.close()
#*************************************************************************************************  

def main():
    return ImageViewApp("Smart Trail Cam", "org.beeware.toga.examples.imageview")


if __name__ == "__main__":
    app = main()
    app.main_loop()

  
  
  
