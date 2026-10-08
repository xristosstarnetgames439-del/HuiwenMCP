#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget widget;
    QGridLayout layout(&widget);
    QLabel label;
    label.setText("Hello World");
    label.setAlignment(Qt::AlignCenter);
    QFont font = label.font();
    font.setPointSize(20);
    font.setBold(true);
    label.setFont(font);
    layout.addWidget(&label);

    widget.resize(400, 400);
    widget.show();

    return app.exec();
}
