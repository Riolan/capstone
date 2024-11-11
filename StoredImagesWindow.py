import toga
from toga import Button
from toga.style import Pack
from toga.style.pack import COLUMN, CENTER, ROW


class StoredImagesWindow():
    
    def StoredImagesWindow_Startup(self):
        #count to iterate through images
        self.image_count = 0   
        self.image_type = "deer"     
               
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

        self.cat_button = toga.Button(
             "Cats",
             enabled=True,
             on_press=self.Cat_Picture,
             style=button_style)

        self.deer_button = toga.Button(
             "Deer",
             enabled=True,
             on_press=self.Deer_Picture,
             style=button_style)

        self.dog_button = toga.Button(
             "Dogs",
             enabled=True,
             on_press=self.Dog_Picture,
             style=button_style)

        self.squirrel_button = toga.Button(
             "Squirrels",
             enabled=True,
             on_press=self.Squirrel_Picture,
             style=button_style)
        
        #Image view to hold image in stored image window
        self.image_view = toga.ImageView(
                    style=Pack(flex = 1, width = 500, direction = COLUMN, alignment=CENTER)
                    )


        self.upper_box = toga.Box(
        style=Pack(direction=ROW),
            children=[
                self.next_button,
                self.prev_button,
                ],
        )
        self.lower_box = toga.Box(
            style = Pack(direction = ROW),
                children = [
                    self.cat_button,
                    self.dog_button,
                    self.squirrel_button,
                    self.deer_button,
                ],
        )

        self.image_box = toga.Box(
            style=Pack(direction=ROW, flex = 1),
            children=[
                toga.Box(style=Pack(flex=1)),
                self.image_view,
                toga.Box(style=Pack(flex=1)),
            ],
        )
        self.outer_box = toga.Box(
            style=Pack(direction=COLUMN, flex=1),
            children=[
                self.upper_box,
                self.lower_box,
                self.image_box,
            ],
        )

        self.stored_image_window = toga.MainWindow()
        self.stored_image_window.content = self.outer_box
        self.stored_image_window.show()   

    def Next_Picture(self, button):
        self.image_count += 1  
        self.prev_button.enabled = True
        if (self.image_count > 1):
            self.next_button.enabled = False
        image_being_viewed = "animalPics/" + self.image_type +'/' + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed
        
    def Prev_Picture(self, button):
        self.image_count -= 1
        self.next_button.enabled = True
        if (self.image_count < 1):
            self.prev_button.enabled = False
        image_being_viewed = "animalPics/" + self.image_type +'/'+ str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed

    def Deer_Picture(self, button):
        self.image_type = "deer"
        image_being_viewed = "animalPics/deer/" + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed

    def Dog_Picture(self, button):
        self.image_type = "dog"
        image_being_viewed = "animalPics/dog/" + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed

    def Cat_Picture(self, button):
        self.image_type = "cat"
        image_being_viewed = "animalPics/cat/" + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed


    def Squirrel_Picture(self, button):
        self.image_type = "squirrel"
        image_being_viewed = "animalPics/squirrel/" + str(self.image_count)+ ".jpg"
        self.image_view.image = image_being_viewed

  
        

          