#include <QDebug>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QTextStream>
#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStack>

QTextStream cin(stdin, QIODevice::ReadOnly);
QTextStream cout(stdout, QIODevice::WriteOnly);
QDir fileDir;

void printErrorMessage(const QString str )
{
    cout << QString::fromUtf8("出错：") << QString::fromUtf8( str.toAscii() )<<endl;
}

bool isEffective ( const QString str,int i )//判断是否被/**/注释掉
{
    QStack<int> stack;
    int pos = str.indexOf("/*");
    while( pos!=-1&&pos<i ){
        if(str[pos]=='*')
            stack.pop_back();
        else
            stack.push(1);
        pos = str.indexOf(QRegExp( "/\\*|\\*/" ), pos+2);
    }
    return stack.isEmpty();
}

int bracketMatching ( const QString str,int i)
{
    QStack<int> stack;
    int pos = str.indexOf("{",i);
    while( pos != -1 ){
        if(str[pos]=='}'){
            stack.pop_back();
            if( stack.isEmpty() )
                break;
        }
        else
            stack.push(1);
        pos = str.indexOf(QRegExp( "{|}" ), pos+1);
    }
    return pos;
}

QString analyzeClass ( QString myclass, QString &myclass_cpp )
{
    QRegExp reg("class\\s+\\w+\\s*(?=:)");
    reg.indexIn( myclass );
    QString className = reg.cap();
    QRegExp reg0 ("\\w+");
    reg0.indexIn( className ,6);
    className = reg0.cap();

    QRegExp reg1("\\n\\s*Q_PROPERTY\\s*\\([^\\)]+");//排除被//注释掉的内容
    int pos=reg1.indexIn(myclass);
    while(pos!=-1){
        if( !isEffective(myclass, pos) )
            continue;
        QString str = reg1.cap();
        pos=reg1.indexIn(myclass, pos+10);
        //qDebug()<<str;
        str = str.right( str.count()-str.indexOf("(")-1 );
        QStringList list;
        foreach(QString i, str.split(" "))
            if(!i.isEmpty())
                list<<i;
        if(list.count()<4) continue;
        if( myclass.indexOf( QRegExp(list[0]+"\\s+"+" m_"+list[1]+"\\s*;" )) ==-1 )
            str = "\r\nprivate:\r\n   "+list[0]+" m_"+list[1]+";\r\n";//改变str的用途，记录要插入.h文件的内容
        else continue;
        bool is=true;

        QString str2="\r\n";//记录要插入cpp文件的内容;
        for(int i=2;i<list.count();++i){
            QString temp = list[i];
            if( temp=="READ" ){
                if ( myclass.indexOf( QRegExp(list[0]+"\\s+"+list[i+1]+"\\s*\\(\\s*\\)\\s*;" )) ==-1 ){
                    str.append( "   "+list[0]+" "+list[++i]+"();\r\n" );
                    str2.append( list[0]+" "+className+"::"+list[i]+"()\r\n{\r\n    return m_"+list[1]+";\r\n}\r\n" );
                }
                else{
                    is = false;
                    break;
                }
            }else if( temp=="WRITE" ){
                if ( myclass.indexOf( QRegExp("void\\s+"+list[i+1]+"\\(\\s*const\\s+"+list[0]+"\\s+[^\\)]*\\)|void\\s+"+list[i+1]+"\\(\\s*"+list[0]+"\\s+[^\\)]*\\)" ))==-1){
                    str.append( "   void "+list[++i]+"(const "+list[0]+" new_"+list[1]+");\r\n" );
                    QString str_temp = "void "+className+"::"+list[i]+"(const "+list[0]+" new_"+list[1]+")\r\n{\r\n";
                    str_temp.append("   if( new_"+list[1]+"!=m_"+list[1]+" ){\r\n      m_"+list[1]+" = "+"new_"+list[1]+";\r\n");
                    int signal_poc = list.indexOf( "NOTIFY" );
                    if( signal_poc!=-1 ){//如果信号存在
                        str_temp.append( "      emit "+list[signal_poc+1]+"();\r\n    }\r\n}\r\n" );
                    }else{
                        str_temp.append( "    }\r\n}\r\n" );
                    }
                    str2.append(str_temp);
                }
                else{
                    is = false;
                    break;
                }
            }else if( temp=="NOTIFY" ){
                if( myclass.indexOf( QRegExp("void\\s+"+list[i+1]+"\\s*\\(\\s*\\)\\s*;" ))==-1 )
                    str.append( "   Q_SIGNAL void "+list[++i]+"();\r\n" );
                else{
                    is = false;
                    break;
                }
            }
        }
        if( is ){
            int class_end_pos = myclass.lastIndexOf("}");
            if( class_end_pos>0 ){
                myclass.insert( class_end_pos, str );
            }
            int pos_temp2 = myclass_cpp.lastIndexOf( QRegExp(className+"\\s*::") );//查找本类在cpp文件中最后的位置
            if( pos_temp2!=-1 ){
                pos_temp2 = bracketMatching( myclass_cpp , pos_temp2);//去查找这个类最后一个函数的结尾}
            }
            if( pos_temp2 == -1 ){
                myclass_cpp.append(str2);
            }else myclass_cpp.insert( pos_temp2 > myclass_cpp.count()-2?myclass_cpp.count():pos_temp2+2,str2 );
        }
    }
    return myclass;
}


void analyzeFile( QString fileName )
{
    QFile file_h (fileDir.path() + "/"+ fileName +".h");
    QFile file_cpp ( fileDir.path() + "/"+ fileName +".cpp" );
    if( file_h.exists() && file_cpp.exists() ){
        cout<<fileName+".h"<<QString::fromUtf8("处理中\n");
        if(file_h.open( QIODevice::ReadWrite )){
            if( file_cpp.open( QIODevice::ReadWrite ) ){
                //qDebug()<<file_h.readAll();
                //qDebug()<<endl<<endl<<file_cpp.readAll();
                QTextStream text_h (&file_h);
                QTextStream text_cpp (&file_cpp);
                QString str_h = text_h.readAll();
                QString str_cpp = text_cpp.readAll();
                QString str_h_temp="";
                int begin = str_h.indexOf("#ifndef");
                while( begin!=-1 )
                {
                    int end  = str_h.indexOf("#endif", begin);
                    int temp = str_h.indexOf("#ifndef", end);
                    str_h_temp.append("\r\n"+analyzeClass( str_h.mid( begin,(temp == -1?str_h.count():temp)- begin ),str_cpp ));
                    begin = temp;
                }
                file_h.resize(0);
                file_cpp.resize(0);
                text_h << str_h_temp;
                text_cpp <<str_cpp;
                file_h.close();
                file_cpp.close();
                //qDebug()<< str_cpp;
            }else printErrorMessage(fileName+".cpp打开失败");
        }else printErrorMessage(fileName+".h打开失败");
    }else{
        printErrorMessage(fileName+".h或者"+fileName+".cpp文件不存在");
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName( QString::fromUtf8("Q_PROPERTY自动化处理") );
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("Star");
    cout<<QString::fromUtf8("Qt Q_PROPERTY自动化处理器，方便为qml注册新属性---雨后星辰\n使用前请键入help命令查看说明\n\n");

    QString str;
    fileDir.cd(QCoreApplication::applicationDirPath());
    while( true ) {
        cout << fileDir.path() << "#" << endl;

        str = cin.readLine();

        QRegExp temp("[a-z]+(?=\\s*)");
        temp.indexIn( str );

        QString order = temp.cap();
        //qDebug()<<order;
        if( !order.isEmpty() ) {
            QString str_temp = str.right( str.count() - str.lastIndexOf(" ")-1 );
            if(order == "help"){
                cout<<QString::fromUtf8( "使用cd path命令改变目录\n使用moc xxx.h命令处理单个文件\n使用moc *.h命令处理所有.h文件\n注意：处理后的新定义变量未初始化\n" );
            }else if(order == "cd"){
                if( !fileDir.cd(str_temp) )
                    printErrorMessage("目录不存在或者不可读");
            }else if( order == "moc" ){
                QStringList list_temp = str_temp.split(".");
                if( list_temp.count() == 2 && list_temp[1] == "h"){
                    if( list_temp[0]!="*" ){
                        analyzeFile( list_temp[0] );
                    }else{
                        QStringList filters;
                        filters << "*.h";
                        foreach( QString i, fileDir.entryList(filters) )
                            analyzeFile( i.left( i.count()-2 ) );
                    }
                }else printErrorMessage("只能处理.h结尾的文件");
            }else printErrorMessage("命令无效");
        }else printErrorMessage("命令无效");
    }
}
