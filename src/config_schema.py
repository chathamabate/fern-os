
from fernconf.FCValue import *
from fernconf.FCTranslator import *
from fernconf.FCSchema import *
from fernconf.FCTooling import *

class FernRange(FCSchema):
    def __init__(self):
        pass


fern_os_config_schema = FCSchemaStruct([
    ("MEM", FCS_INT)
])

if __name__ == "__main__":
    run_fernconf(fern_os_config_schema, gcc=FCT_GCC, make=FCT_MAKE, ld=FCT_LD32, gas=FCT_GAS)



