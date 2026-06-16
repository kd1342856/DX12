import codecs

try:
    with codecs.open(r'C:\GitHub\DX12\Source\Graphics\Device\GraphicsDevice.cpp', 'r', encoding='cp932') as f:
        lines = f.readlines()
        
    for i in range(len(lines)):
        if 'm_fenceEvent = CreateEvent' in lines[i]:
            lines[i+2] = '\t\tassert(0 && "Event creation failed");\r\n'
            
    with codecs.open(r'C:\GitHub\DX12\Source\Graphics\Device\GraphicsDevice.cpp', 'w', encoding='cp932') as f:
        f.writelines(lines)
    print("Fixed GraphicsDevice.cpp!")
except Exception as e:
    print(e)
