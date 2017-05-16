#include "customcontrols.h"
#include "ui_customcontrols.h"

CustomControls::CustomControls(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CustomControls)
{
    ui->setupUi(this);
}

CustomControls::~CustomControls()
{
    delete ui;
}
