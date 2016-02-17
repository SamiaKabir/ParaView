/*=========================================================================

   Program: ParaView
   Module:  pqExportViewSelection.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "vtkCamera.h"
#include "vtkSMProxyManager.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMSessionProxyManager.h"

#include "pqExportViewSelection.h"
#include "ui_pqExportViewSelection.h"
#include "pqRenderView.h"
#include "pqContextView.h"
#include "pqImageOutputInfo.h"


// ----------------------------------------------------------------------------
pqExportViewSelection::pqExportViewSelection(QWidget* parent_)
: QWidget(parent_)
, Ui(new Ui::ExportViewSelection())
{
  this->Ui->setupUi(this);

  connect(this->Ui->pbPrevious, SIGNAL(clicked()), this, SLOT(onPreviousClicked()));
  connect(this->Ui->pbNext, SIGNAL(clicked()), this, SLOT(onNextClicked()));
}

pqExportViewSelection::~pqExportViewSelection()
{
  delete this->Ui;
}

void pqExportViewSelection::onPreviousClicked()
{
  int previousIndex = this->Ui->swViews->currentIndex() - 1;
  if (previousIndex >= 0)
    {
    this->Ui->swViews->setCurrentIndex(previousIndex);
    if (previousIndex == 0) // first view
      {
      this->Ui->pbPrevious->setEnabled(false);
      }

    if (!this->Ui->pbNext->isEnabled())
      {
      this->Ui->pbNext->setEnabled(true);
      }
    }
}

void pqExportViewSelection::onNextClicked()
{
  int nextIndex = this->Ui->swViews->currentIndex() + 1;
  if (nextIndex < this->Ui->swViews->count())
    {
    this->Ui->swViews->setCurrentIndex(nextIndex);
    if (nextIndex == this->Ui->swViews->count() - 1) // last view
      {
      this->Ui->pbNext->setEnabled(false);
      }

    if (!this->Ui->pbPrevious->isEnabled())
      {
      this->Ui->pbPrevious->setEnabled(true);
      }
    }
}

void pqExportViewSelection::populateViews(QList<pqRenderViewBase*> const &  renderViews,
  QList<pqContextView*> const & contextViews)
{
  int const  numberOfViews = renderViews.size() + contextViews.size();

  // first do 2D and 3D render views
  this->addViews<QList<pqRenderViewBase*> >(renderViews, numberOfViews);
  this->addViews<QList<pqContextView*> >(contextViews, numberOfViews);

  if (this->Ui->swViews->count() > 1)
    {
    this->Ui->swViews->setCurrentIndex(0);
    this->Ui->pbNext->setEnabled(true);
    }
}

template <typename T>
void pqExportViewSelection::addViews(T const & views, int numberOfViews)
{
  Qt::WindowFlags parentFlags = this->Ui->swViews->windowFlags();
  int viewCounter = this->Ui->swViews->count();

  for(typename T::ConstIterator it = views.begin() ; it != views.end() ; it++)
    {
    QString viewName = numberOfViews == 1 ? "image_%t.png" :
                       QString("image_%1_%t.png").arg(viewCounter++);

    pqImageOutputInfo* info = new pqImageOutputInfo(this->Ui->swViews, parentFlags, *it,  viewName);
    this->Ui->swViews->addWidget(info);
    }
}

QList<pqImageOutputInfo*> pqExportViewSelection::getImageOutputInfos()
{
  QList<pqImageOutputInfo*> infos;

  for (int i = 0 ; i < this->Ui->swViews->count() ; i++)
    {
    if (pqImageOutputInfo* qinfo = qobject_cast<pqImageOutputInfo*>(
          this->Ui->swViews->widget(i)) )
      {
      infos.append(qinfo);
      }
    }

  return infos;
}

void pqExportViewSelection::setCinemaVisible(bool status, bool specASupport)
{
  typedef pqImageOutputInfo* InfoPtr;

  int const size_ = this->Ui->swViews->count();
  for (int i = 0 ; i < size_ ; i++)
    {
    if (InfoPtr info = qobject_cast<InfoPtr>(this->Ui->swViews->widget(i)))
      {
        info->setCinemaVisible(status, specASupport);
      }
    }
}

QString pqExportViewSelection::getSelectionAsPythonScript(QString const & scriptFormat, bool isComposite)
{
  QString rendering_info;

  vtkSMSessionProxyManager* proxyManager =
      vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();

  QList<pqImageOutputInfo*> allViews = this->getImageOutputInfos();
  for(int index = 0; index < allViews.count(); index++)
    {
    pqImageOutputInfo* const & viewInfo = allViews.at(index);
    pqView* view = viewInfo->getView();
    QSize viewSize = view->getSize();
    vtkSMViewProxy* viewProxy = view->getViewProxy();
    vtkSMRenderViewProxy* rvp = vtkSMRenderViewProxy::SafeDownCast(viewProxy);
    pqRenderView* rview = dynamic_cast<pqRenderView*>(view);

    //cinema camera parameters
    QString cinemaCam = "{}";
    QString camType = viewInfo->getCameraType();
    if (rvp && (camType != "None"))
      {
      cinemaCam = QString("{");

      if (isComposite)
        {
        cinemaCam += "\"composite\":True, ";
        }
      cinemaCam += "\"camera\":\"";
      cinemaCam += camType;
      cinemaCam += "\"";
      if (camType != "Static")
        {
        cinemaCam += ", ";

        cinemaCam += "\"phi\":[";
        int j;
        for (j = -180; j < 180; j+= (360/viewInfo->getPhi()))
          {
          cinemaCam += QString::number(j) + ",";
          }
        cinemaCam.chop(1);
        cinemaCam += "],";

        cinemaCam += "\"theta\":[";
        for (j = -180; j < 180; j+= (360/viewInfo->getTheta()))
          {
          cinemaCam += QString::number(j) + ",";
          }
        cinemaCam.chop(1);
        cinemaCam += "], ";

        vtkCamera *cam = rvp->GetActiveCamera();
        double eye[3];
        double at[3];
        double up[3];
        cam->GetPosition(eye);
        rview->getCenterOfRotation(at);
        cam->GetViewUp(up);
        cinemaCam += "\"initial\":{ ";
        cinemaCam += "\"eye\": [" +
          QString::number(eye[0]) + "," + QString::number(eye[1]) + "," + QString::number(eye[2]) + "], ";
        cinemaCam += "\"at\": [" +
          QString::number(at[0]) + "," + QString::number(at[1]) + "," + QString::number(at[2]) + "], ";
        cinemaCam += "\"up\": [" +
          QString::number(up[0]) + "," + QString::number(up[1]) + "," + QString::number(up[2]) + "] ";
        cinemaCam += "} ";
        }
      cinemaCam += "}";
      }

    // apply specified format
    rendering_info += scriptFormat.
      arg(proxyManager->GetProxyName("views", viewProxy)).
      arg(viewInfo->getImageFileName()).arg(viewInfo->getWriteFrequency()).
      arg(static_cast<int>(viewInfo->fitToScreen())).
      arg(viewInfo->getMagnification()).
      arg(viewSize.width()).
      arg(viewSize.height()).
      arg(cinemaCam);

    if (index < allViews.count() - 1)
      rendering_info += ", ";
    }

  return rendering_info;
}