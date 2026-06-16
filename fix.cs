using System;
using System.IO;
using System.Text;

class Program
{
    static void Main()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        var enc = Encoding.GetEncoding(932);
        
        string[] lines = File.ReadAllLines(@"C:\GitHub\DX12\Source\Graphics\Device\GraphicsDevice.cpp", enc);
        for(int i = 0; i < lines.Length; i++)
        {
            if(lines[i].Contains("m_fenceEvent = CreateEvent"))
            {
                lines[i+2] = "\t\tassert(0 && \"Event creation failed\");";
            }
        }
        File.WriteAllLines(@"C:\GitHub\DX12\Source\Graphics\Device\GraphicsDevice.cpp", lines, enc);
    }
}
