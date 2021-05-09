# Configuration Parser Class for the VXAPP Format
import os
import json



class VXAppConfig:
    def __init__(self, path_to_config_file, selected_configuration_name="default"):
        self.valid = False
        self.name = "Untitled App"
        self.configurations = []

        try:
            with open(path_to_config_file,"r") as f:
                data = json.load(f)
                self.name = data.get("name","Untitled App")
                self.configurations = data['ep']
        except Exception as e:
            print(f"Error Loading App Config: {e}")
            return
        # Set Selected Configuration
        self.selected_configuration = self.select_configuration(selected_configuration_name)
        if not self.selected_configuration:
            print("Error Selecting Configuration")
            return
        self.valid = True
    def select_configuration(self, name):
        for config in self.configurations:
            if config['desc'] == name:
                return config
        return None