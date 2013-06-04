package;

import gl3font.Font;
import gl3font.GLString;
import ogl.GLM;

class Main {
    static function main() {
        var c1:Vec4 = [1,1,1,0];
        var c2:Vec4 = [1,1,0,0];
        var c3:Vec4 = [1,0,0,0];
        var x:GLString = "cwd=/home/luca/Projects/stress/";x += "\n   ";
        x += GLString.make("2011.07.21-f4_12ms.avi",c1)  ; x +=  "\n   ";
        x += GLString.make("0303_11Mavi.chd",c2)  ; x +=  "\n   ";
        x += GLString.make("2011.06.30-a1_5cmavi.chd",c3)  ; x +=  "\n   ";
        x += GLString.make("2011.07.12-d4_8ms.avi",c1)  ; x +=  "\n   ";
        x += GLString.make("2011.07.12-d4_8msavi.chd",c2)  ; x +=  "\n   ";
        x += GLString.make("2011.06.30-a1_5cm.avi",c3)  ; x +=  "\n   ";
        x += GLString.make("2011.07.13-e4_12msavi.chd",c1)  ; x +=  "\n   ";
        x += GLString.make("0303_11M.avi",c2)  ; x +=  "\n   ";
        x += GLString.make("2011.07.13-e4_12ms.avi",c3)  ; x +=  "\n   ";
        x += GLString.make("0303_5m.avi",c1); x+= "\n   ";
        x += GLString.make("...",c2);
        trace("\n"+x.toString());
        for (l in StringBuffer.getLines(x)) trace(l.toString());
    }
}
