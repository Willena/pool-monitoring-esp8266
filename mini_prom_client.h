#ifndef MINI_PROM_CLIENT
#define MINI_PROM_CLIENT

#define GAUGE "gauge"
#define SUMMARY "summary"
#define COUNTER "counter"

class MiniPromClient
{
private:
    String o1;

    void writeDesc(String name, String help){
        this->o1 += "# HELP " + name + " " + help + "\n";
    };

    void writeType(String &name, const char * type){
        this->o1 += "# TYPE " + name + " " + String(type) + "\n";
    };

public:
    MiniPromClient(){
    };

    void put(String name, String labels,  String value , String help , const char * type){
        writeDesc(name, help);
        writeType(name, type);
        put(name, value, labels);
    };

    void put(String name, String value){ 
        put(name,"", value);
    }

    void put(String name, String labels, String value){
        this->o1 += "" + name + "{" +labels + "} " + value +"\n";
    }

    String getMessage(){
      return o1;
    }
};


#endif
