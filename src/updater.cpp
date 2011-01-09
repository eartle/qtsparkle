/* This file is part of qtsparkle.
   Copyright (c) 2010 David Sansome <me@davidsansome.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "common.h"
#include "uicontroller.h"
#include "updatechecker.h"
#include "updater.h"

#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QtDebug>


namespace qtsparkle {

struct Updater::Private {
  Private(const QUrl& appcast_url, QWidget* parent_widget, Updater* updater)
    : parent_widget_(parent_widget),
      updater_(updater),
      network_(NULL),
      appcast_url_(appcast_url),
      check_automatically_(false),
      first_boot_(true),
      asked_permission_(false)
  {
  }

  void AskPermission();
  void CheckNow(bool quiet);

  QWidget* parent_widget_;
  Updater* updater_;

  QNetworkAccessManager* network_;
  QIcon icon_;
  QString version_;

  QUrl appcast_url_;
  bool check_automatically_;
  bool first_boot_;
  bool asked_permission_;

  QEvent::Type ask_permission_event_;
  QEvent::Type auto_check_event_;
};


Updater::Updater(const QUrl& appcast_url, QWidget* parent)
  : QObject(parent),
    d(new Private(appcast_url, parent, this))
{
  d->ask_permission_event_ = QEvent::Type(QEvent::registerEventType());
  d->auto_check_event_ = QEvent::Type(QEvent::registerEventType());
  d->version_ = QCoreApplication::applicationVersion();

  if (parent) {
    SetIcon(parent->windowIcon());
  }

  // Load settings
  QSettings s;
  s.beginGroup(kSettingsGroup);
  d->check_automatically_ = s.value("check_automatically", false).toBool();
  d->first_boot_ = s.value("first_boot", true).toBool();
  d->asked_permission_ = s.value("asked_permission", false).toBool();

  // Should we ask for permission now?
  if (!d->asked_permission_ && !d->first_boot_) {
    QCoreApplication::postEvent(this, new QEvent(d->ask_permission_event_));
  }

  // Or check for updates?
  if (d->check_automatically_) {
    QCoreApplication::postEvent(this, new QEvent(d->auto_check_event_));
  }

  // Turn off the first boot flag if it's on
  if (d->first_boot_) {
    d->first_boot_ = false;
    s.setValue("first_boot", false);
  }
}

Updater::~Updater() {
}

void Updater::SetIcon(const QIcon& icon) {
  d->icon_ = icon;
}

void Updater::SetNetworkAccessManager(QNetworkAccessManager* network) {
  d->network_ = network;
}

void Updater::SetVersion(const QString& version) {
  d->version_ = version;
}

bool Updater::event(QEvent* e) {
  if (e->type() == d->auto_check_event_) {
    d->CheckNow(true);
    return true;
  }

  if (e->type() == d->ask_permission_event_) {
    d->AskPermission();
    return true;
  }

  return QObject::event(e);
}

void Updater::Private::AskPermission() {
  QMessageBox box(QMessageBox::Question,
    tr("Check for updates automatically?"),
    tr("Should %1 automatically check for updates?  You can always check for updates manually from the menu.")
        .arg(qApp->applicationName()), QMessageBox::NoButton, parent_widget_);
  QPushButton* yes = box.addButton(tr("Check automatically"), QMessageBox::YesRole);
  box.addButton(tr("Don't check"), QMessageBox::NoRole);

  box.exec();
  check_automatically_ = (box.clickedButton() == yes);

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("asked_permission", true);
  s.setValue("check_automatically", check_automatically_);

  if (check_automatically_) {
    CheckNow(true);
  }
}

void Updater::CheckNow() {
  d->CheckNow(false);
}

void Updater::Private::CheckNow(bool quiet) {
  UiController* controller = new UiController(quiet, updater_, parent_widget_);
  controller->SetNetworkAccessManager(network_);
  controller->SetIcon(icon_);
  controller->SetVersion(version_);

  UpdateChecker* checker = new UpdateChecker(updater_);
  checker->SetNetworkAccessManager(network_);
  checker->SetVersion(version_);

  connect(checker, SIGNAL(CheckStarted()), controller, SLOT(CheckStarted()));
  connect(checker, SIGNAL(CheckFailed(QString)), controller, SLOT(CheckFailed(QString)));
  connect(checker, SIGNAL(UpdateAvailable(AppCastPtr)), controller, SLOT(UpdateAvailable(AppCastPtr)));
  connect(checker, SIGNAL(UpToDate()), controller, SLOT(UpToDate()));

  checker->Check(appcast_url_, !quiet);

  // The UiController will delete itself when the check is finished.
}

} // namespace qtsparkle
