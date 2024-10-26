import toga
from toga import Button
from toga.style import Pack
from toga.style.pack import COLUMN, CENTER, ROW


class StoredImagesWindow():
    
    def StoredImagesWindow_Startup(self):
        #count to iterate through images
        self.image_count = 0        
               
        button_style = Pack(flex=1)

        self.next_button = toga.Button( 
            "Next Image", 
            on_press=self.Next_Picture, 
            style=button_style)
            
        self.prev_button = toga.Button(
             "Previous Image",
             enabled=False,
             on_press=self.Prev_Picture,
             style=button_style)
        
        #Image view to hold image in stored image window
        self.image_view = toga.ImageView(
                    style=Pack(flex=1, width=300, alignment=CENTER)
                    )
        image_being_viewed = ("animalPics/0.jpg")
        self.image_view.image = image_being_viewed


        self.inner_box = toga.Box(
        style=Pack(direction=ROW),
            children=[
                self.next_button,
                self.prev_button,
                ],
        )

        self.outer_box = toga.Box(
            style=Pack(direction=COLUMN, flex=1),
            children=[
                self.inner_box,
                self.image_view, 
                toga.Label(text="Animal Picture", style=Pack(text_align=CENTER)),
            ],
        )

        self.stored_image_window = toga.MainWindow()
        self.stored_image_window.content = self.outer_box
        self.stored_image_window.show()   

    def Next_Picture(self, button):
        self.image_count += 1  
        self.prev_button.enabled = True
        image_being_viewed = ("animalPics/" + str(self.image_count)+ ".jpg")
        if (self.image_count > 3):
            self.next_button.enabled = False
            self.prev_button.enabled = True
        image_being_viewed = "animalPics/" + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed
        
    def Prev_Picture(self, button):
        self.image_count -= 1
        if (self.image_count < 1):
            self.prev_button.ennabled = False
            self.next_button.enabled = True
        image_being_viewed = "animalPics/" + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed


  
        

          