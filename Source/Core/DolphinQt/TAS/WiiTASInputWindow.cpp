// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Swap.h"

#include "Core/Core.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "Core/HW/WiimoteEmu/Camera.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/TAS/BalanceBoardWidget.h"
#include "DolphinQt/TAS/IRWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/WiiTASInputWindow.h"

#include "InputCommon/InputConfig.h"

using namespace WiimoteCommon;

WiiTASInputWindow::WiiTASInputWindow(QWidget* parent, int num) : TASInputWindow(parent), m_num(num)
{
  const QKeySequence ir_x_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_F);
  const QKeySequence ir_y_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_G);

  m_ir_box = new QGroupBox(QStringLiteral("%1 (%2/%3)")
                               .arg(tr("IR"),
                                    ir_x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                                    ir_y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  const int ir_x_default = static_cast<int>(std::round(IRWidget::ir_max_x / 2.));
  const int ir_y_default = static_cast<int>(std::round(IRWidget::ir_max_y / 2.));

  auto* x_layout = new QHBoxLayout;
  m_ir_x_value = CreateSliderValuePair(x_layout, ir_x_default, IRWidget::ir_max_x,
                                       ir_x_shortcut_key_sequence, Qt::Horizontal, m_ir_box, true);

  auto* y_layout = new QVBoxLayout;
  m_ir_y_value = CreateSliderValuePair(y_layout, ir_y_default, IRWidget::ir_max_y,
                                       ir_y_shortcut_key_sequence, Qt::Vertical, m_ir_box, true);
  m_ir_y_value->setMaximumWidth(60);

  auto* visual = new IRWidget(this);
  visual->SetX(ir_x_default);
  visual->SetY(ir_y_default);

  connect(m_ir_x_value, qOverload<int>(&QSpinBox::valueChanged), visual, &IRWidget::SetX);
  connect(m_ir_y_value, qOverload<int>(&QSpinBox::valueChanged), visual, &IRWidget::SetY);
  connect(visual, &IRWidget::ChangedX, m_ir_x_value, &QSpinBox::setValue);
  connect(visual, &IRWidget::ChangedY, m_ir_y_value, &QSpinBox::setValue);

  auto* visual_ar = new AspectRatioWidget(visual, IRWidget::ir_max_x, IRWidget::ir_max_y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* ir_layout = new QVBoxLayout;
  ir_layout->addLayout(x_layout);
  ir_layout->addLayout(visual_layout);
  m_ir_box->setLayout(ir_layout);

  m_nunchuk_stick_box = CreateStickInputs(tr("Nunchuk Stick"), m_nunchuk_stick_x_value,
                                          m_nunchuk_stick_y_value, 255, 255, Qt::Key_X, Qt::Key_Y);

  m_classic_left_stick_box =
      CreateStickInputs(tr("Left Stick"), m_classic_left_stick_x_value,
                        m_classic_left_stick_y_value, 63, 63, Qt::Key_F, Qt::Key_G);

  m_classic_right_stick_box =
      CreateStickInputs(tr("Right Stick"), m_classic_right_stick_x_value,
                        m_classic_right_stick_y_value, 31, 31, Qt::Key_Q, Qt::Key_W);

  const QKeySequence balance_tl_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_L);
  const QKeySequence balance_tr_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_R);
  const QKeySequence balance_bl_shortcut_key_sequence =
      QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_L);
  const QKeySequence balance_br_shortcut_key_sequence =
      QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_R);
  const QKeySequence balance_weight_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_W);

  m_balance_board_box = new QGroupBox(
      QStringLiteral("%1 (%2/%3/%4)")
          .arg(tr("Balance"), balance_tl_shortcut_key_sequence.toString(QKeySequence::NativeText),
               balance_tr_shortcut_key_sequence.toString(QKeySequence::NativeText),
               balance_weight_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  auto* bal_top_layout = new QHBoxLayout;
  m_top_left_balance_value = CreateWeightSliderValuePair(
      bal_top_layout, -34, 68, balance_tl_shortcut_key_sequence, m_balance_board_box);
  m_top_right_balance_value = CreateWeightSliderValuePair(
      bal_top_layout, -34, 68, balance_tr_shortcut_key_sequence, m_balance_board_box);

  auto* bal_bottom_layout = new QHBoxLayout;
  m_bottom_left_balance_value = CreateWeightSliderValuePair(
      bal_bottom_layout, -34, 68, balance_bl_shortcut_key_sequence, m_balance_board_box);
  m_bottom_right_balance_value = CreateWeightSliderValuePair(
      bal_bottom_layout, -34, 68, balance_br_shortcut_key_sequence, m_balance_board_box);

  auto* bal_weight_layout = new QHBoxLayout;
  m_total_weight_value = CreateWeightSliderValuePair(
      bal_weight_layout, 0, 136, balance_weight_shortcut_key_sequence, m_balance_board_box);

  auto* bal_visual = new BalanceBoardWidget(this);
  connect(m_top_right_balance_value,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), bal_visual,
          &BalanceBoardWidget::SetTR);
  connect(m_bottom_right_balance_value,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), bal_visual,
          &BalanceBoardWidget::SetBR);
  connect(m_top_left_balance_value,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), bal_visual,
          &BalanceBoardWidget::SetTL);
  connect(m_bottom_left_balance_value,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), bal_visual,
          &BalanceBoardWidget::SetBL);
  connect(bal_visual, &BalanceBoardWidget::ChangedTR, m_top_right_balance_value,
          &QDoubleSpinBox::setValue);
  connect(bal_visual, &BalanceBoardWidget::ChangedBR, m_bottom_right_balance_value,
          &QDoubleSpinBox::setValue);
  connect(bal_visual, &BalanceBoardWidget::ChangedTL, m_top_left_balance_value,
          &QDoubleSpinBox::setValue);
  connect(bal_visual, &BalanceBoardWidget::ChangedBL, m_bottom_left_balance_value,
          &QDoubleSpinBox::setValue);

  constexpr double DEFAULT_WEIGHT = 63.5;
  m_top_right_balance_value->setValue(DEFAULT_WEIGHT / 4);
  m_bottom_right_balance_value->setValue(DEFAULT_WEIGHT / 4);
  m_top_left_balance_value->setValue(DEFAULT_WEIGHT / 4);
  m_bottom_left_balance_value->setValue(DEFAULT_WEIGHT / 4);

  connect(m_total_weight_value,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), bal_visual,
          &BalanceBoardWidget::SetTotal);
  connect(bal_visual, &BalanceBoardWidget::ChangedTotal, m_total_weight_value,
          &QDoubleSpinBox::setValue);
  m_total_weight_value->setValue(DEFAULT_WEIGHT);

  auto* bal_ar = new AspectRatioWidget(bal_visual, 20, 12);
  bal_ar->setMinimumHeight(120);
  auto* bal_visual_layout = new QHBoxLayout;
  bal_visual_layout->addWidget(bal_ar);

  auto* bal_layout = new QVBoxLayout;
  bal_layout->addLayout(bal_top_layout);
  bal_layout->addLayout(bal_visual_layout);
  bal_layout->addLayout(bal_bottom_layout);
  bal_layout->addLayout(bal_weight_layout);
  m_balance_board_box->setLayout(bal_layout);

  // Need to enforce the same minimum width because otherwise the different lengths in the labels
  // used on the QGroupBox will cause the StickWidgets to have different sizes.
  m_ir_box->setMinimumWidth(20);
  m_nunchuk_stick_box->setMinimumWidth(20);

  m_remote_orientation_box = new QGroupBox(tr("Wii Remote Orientation"));

  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(m_ir_box);
  top_layout->addWidget(m_nunchuk_stick_box);
  top_layout->addWidget(m_classic_left_stick_box);
  top_layout->addWidget(m_classic_right_stick_box);
  top_layout->addWidget(m_balance_board_box);

  auto* remote_orientation_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), m_remote_orientation_x_value, 512, 1023, Qt::Key_Q,
                                  m_remote_orientation_box);
  auto* remote_orientation_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), m_remote_orientation_y_value, 512, 1023, Qt::Key_W,
                                  m_remote_orientation_box);
  auto* remote_orientation_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), m_remote_orientation_z_value, 616, 1023, Qt::Key_E,
                                  m_remote_orientation_box);

  auto* remote_orientation_layout = new QVBoxLayout;
  remote_orientation_layout->addLayout(remote_orientation_x_layout);
  remote_orientation_layout->addLayout(remote_orientation_y_layout);
  remote_orientation_layout->addLayout(remote_orientation_z_layout);
  m_remote_orientation_box->setLayout(remote_orientation_layout);

  m_nunchuk_orientation_box = new QGroupBox(tr("Nunchuk Orientation"));

  auto* nunchuk_orientation_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), m_nunchuk_orientation_x_value, 512, 1023, Qt::Key_I,
                                  m_nunchuk_orientation_box);
  auto* nunchuk_orientation_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), m_nunchuk_orientation_y_value, 512, 1023, Qt::Key_O,
                                  m_nunchuk_orientation_box);
  auto* nunchuk_orientation_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), m_nunchuk_orientation_z_value, 512, 1023, Qt::Key_P,
                                  m_nunchuk_orientation_box);

  auto* nunchuk_orientation_layout = new QVBoxLayout;
  nunchuk_orientation_layout->addLayout(nunchuk_orientation_x_layout);
  nunchuk_orientation_layout->addLayout(nunchuk_orientation_y_layout);
  nunchuk_orientation_layout->addLayout(nunchuk_orientation_z_layout);
  m_nunchuk_orientation_box->setLayout(nunchuk_orientation_layout);

  m_triggers_box = new QGroupBox(tr("Triggers"));
  auto* l_trigger_layout = CreateSliderValuePairLayout(tr("Left"), m_left_trigger_value, 0, 31,
                                                       Qt::Key_N, m_triggers_box);
  auto* r_trigger_layout = CreateSliderValuePairLayout(tr("Right"), m_right_trigger_value, 0, 31,
                                                       Qt::Key_M, m_triggers_box);

  auto* triggers_layout = new QVBoxLayout;
  triggers_layout->addLayout(l_trigger_layout);
  triggers_layout->addLayout(r_trigger_layout);
  m_triggers_box->setLayout(triggers_layout);

  m_a_button = CreateButton(QStringLiteral("&A"));
  m_b_button = CreateButton(QStringLiteral("&B"));
  m_1_button = CreateButton(QStringLiteral("&1"));
  m_2_button = CreateButton(QStringLiteral("&2"));
  m_plus_button = CreateButton(QStringLiteral("&+"));
  m_minus_button = CreateButton(QStringLiteral("&-"));
  m_home_button = CreateButton(QStringLiteral("&HOME"));
  m_left_button = CreateButton(QStringLiteral("&Left"));
  m_up_button = CreateButton(QStringLiteral("&Up"));
  m_down_button = CreateButton(QStringLiteral("&Down"));
  m_right_button = CreateButton(QStringLiteral("&Right"));
  m_c_button = CreateButton(QStringLiteral("&C"));
  m_z_button = CreateButton(QStringLiteral("&Z"));

  auto* buttons_layout = new QGridLayout;
  buttons_layout->addWidget(m_a_button, 0, 0);
  buttons_layout->addWidget(m_b_button, 0, 1);
  buttons_layout->addWidget(m_1_button, 0, 2);
  buttons_layout->addWidget(m_2_button, 0, 3);
  buttons_layout->addWidget(m_plus_button, 0, 4);
  buttons_layout->addWidget(m_minus_button, 0, 5);

  buttons_layout->addWidget(m_home_button, 1, 0);
  buttons_layout->addWidget(m_left_button, 1, 1);
  buttons_layout->addWidget(m_up_button, 1, 2);
  buttons_layout->addWidget(m_down_button, 1, 3);
  buttons_layout->addWidget(m_right_button, 1, 4);

  buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding), 0, 7);

  m_remote_buttons_box = new QGroupBox(tr("Wii Remote Buttons"));
  m_remote_buttons_box->setLayout(buttons_layout);

  auto* nunchuk_buttons_layout = new QHBoxLayout;
  nunchuk_buttons_layout->addWidget(m_c_button);
  nunchuk_buttons_layout->addWidget(m_z_button);
  nunchuk_buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

  m_nunchuk_buttons_box = new QGroupBox(tr("Nunchuk Buttons"));
  m_nunchuk_buttons_box->setLayout(nunchuk_buttons_layout);

  m_classic_a_button = CreateButton(QStringLiteral("&A"));
  m_classic_b_button = CreateButton(QStringLiteral("&B"));
  m_classic_x_button = CreateButton(QStringLiteral("&X"));
  m_classic_y_button = CreateButton(QStringLiteral("&Y"));
  m_classic_l_button = CreateButton(QStringLiteral("&L"));
  m_classic_r_button = CreateButton(QStringLiteral("&R"));
  m_classic_zl_button = CreateButton(QStringLiteral("&ZL"));
  m_classic_zr_button = CreateButton(QStringLiteral("ZR"));
  m_classic_plus_button = CreateButton(QStringLiteral("&+"));
  m_classic_minus_button = CreateButton(QStringLiteral("&-"));
  m_classic_home_button = CreateButton(QStringLiteral("&HOME"));
  m_classic_left_button = CreateButton(QStringLiteral("L&eft"));
  m_classic_up_button = CreateButton(QStringLiteral("&Up"));
  m_classic_down_button = CreateButton(QStringLiteral("&Down"));
  m_classic_right_button = CreateButton(QStringLiteral("R&ight"));

  auto* classic_buttons_layout = new QGridLayout;
  classic_buttons_layout->addWidget(m_classic_a_button, 0, 0);
  classic_buttons_layout->addWidget(m_classic_b_button, 0, 1);
  classic_buttons_layout->addWidget(m_classic_x_button, 0, 2);
  classic_buttons_layout->addWidget(m_classic_y_button, 0, 3);
  classic_buttons_layout->addWidget(m_classic_l_button, 0, 4);
  classic_buttons_layout->addWidget(m_classic_r_button, 0, 5);
  classic_buttons_layout->addWidget(m_classic_zl_button, 0, 6);
  classic_buttons_layout->addWidget(m_classic_zr_button, 0, 7);

  classic_buttons_layout->addWidget(m_classic_plus_button, 1, 0);
  classic_buttons_layout->addWidget(m_classic_minus_button, 1, 1);
  classic_buttons_layout->addWidget(m_classic_home_button, 1, 2);
  classic_buttons_layout->addWidget(m_classic_left_button, 1, 3);
  classic_buttons_layout->addWidget(m_classic_up_button, 1, 4);
  classic_buttons_layout->addWidget(m_classic_down_button, 1, 5);
  classic_buttons_layout->addWidget(m_classic_right_button, 1, 6);

  classic_buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding), 0, 8);

  m_classic_buttons_box = new QGroupBox(tr("Classic Buttons"));
  m_classic_buttons_box->setLayout(classic_buttons_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout);
  layout->addWidget(m_remote_orientation_box);
  layout->addWidget(m_nunchuk_orientation_box);
  layout->addWidget(m_triggers_box);
  layout->addWidget(m_remote_buttons_box);
  layout->addWidget(m_nunchuk_buttons_box);
  layout->addWidget(m_classic_buttons_box);
  layout->addWidget(m_settings_box);

  setLayout(layout);

  WiimoteEmu::ExtensionNumber ext = WiimoteEmu::ExtensionNumber::NONE;
  if (Core::IsRunning())
  {
    ext = static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(num))
              ->GetActiveExtensionNumber();
  }
  else if (num == WIIMOTE_BALANCE_BOARD)
  {
    ext = WiimoteEmu::ExtensionNumber::BALANCE_BOARD;
  }
  else
  {
    IniFile ini;
    ini.Load(File::GetUserPath(D_CONFIG_IDX) + "WiimoteNew.ini");
    std::string extension;
    ini.GetIfExists("Wiimote" + std::to_string(num + 1), "Extension", &extension);

    if (extension == "Nunchuk")
      ext = WiimoteEmu::ExtensionNumber::NUNCHUK;
    if (extension == "Classic")
      ext = WiimoteEmu::ExtensionNumber::CLASSIC;
  }
  UpdateExt(ext);
}

void WiiTASInputWindow::UpdateExt(WiimoteEmu::ExtensionNumber ext)
{
  if (ext == WiimoteEmu::ExtensionNumber::NUNCHUK)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote + Nunchuk").arg(m_num + 1));
    m_ir_box->show();
    m_nunchuk_stick_box->show();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    m_remote_orientation_box->show();
    m_nunchuk_orientation_box->show();
    m_triggers_box->hide();
    m_nunchuk_buttons_box->show();
    m_remote_buttons_box->show();
    m_classic_buttons_box->hide();
    m_balance_board_box->hide();
  }
  else if (ext == WiimoteEmu::ExtensionNumber::CLASSIC)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Classic Controller").arg(m_num + 1));
    m_ir_box->hide();
    m_nunchuk_stick_box->hide();
    m_classic_right_stick_box->show();
    m_classic_left_stick_box->show();
    m_remote_orientation_box->hide();
    m_nunchuk_orientation_box->hide();
    m_triggers_box->show();
    m_remote_buttons_box->hide();
    m_nunchuk_buttons_box->hide();
    m_classic_buttons_box->show();
    m_balance_board_box->hide();
  }
  else if (ext == WiimoteEmu::ExtensionNumber::BALANCE_BOARD)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Balance Board").arg(m_num + 1));
    m_ir_box->hide();
    m_nunchuk_stick_box->hide();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    m_remote_orientation_box->hide();
    m_nunchuk_orientation_box->hide();
    m_triggers_box->hide();
    m_remote_buttons_box->show();
    m_nunchuk_buttons_box->hide();
    m_classic_buttons_box->hide();
    m_balance_board_box->show();
  }
  else
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote").arg(m_num + 1));
    m_ir_box->show();
    m_nunchuk_stick_box->hide();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    m_remote_orientation_box->show();
    m_nunchuk_orientation_box->hide();
    m_triggers_box->hide();
    m_remote_buttons_box->show();
    m_nunchuk_buttons_box->hide();
    m_classic_buttons_box->hide();
    m_balance_board_box->hide();
  }
}

void WiiTASInputWindow::GetValues(DataReportBuilder& rpt, WiimoteEmu::ExtensionNumber ext,
                                  const WiimoteEmu::EncryptionKey& key)
{
  if (!isVisible())
    return;

  UpdateExt(ext);

  if (m_remote_buttons_box->isVisible() && rpt.HasCore())
  {
    DataReportBuilder::CoreData core;
    rpt.GetCoreData(&core);

    using EmuWiimote = WiimoteEmu::Wiimote;

    u16& buttons = core.hex;
    GetButton<u16>(m_a_button, buttons, EmuWiimote::BUTTON_A);
    GetButton<u16>(m_b_button, buttons, EmuWiimote::BUTTON_B);
    GetButton<u16>(m_1_button, buttons, EmuWiimote::BUTTON_ONE);
    GetButton<u16>(m_2_button, buttons, EmuWiimote::BUTTON_TWO);
    GetButton<u16>(m_plus_button, buttons, EmuWiimote::BUTTON_PLUS);
    GetButton<u16>(m_minus_button, buttons, EmuWiimote::BUTTON_MINUS);
    GetButton<u16>(m_home_button, buttons, EmuWiimote::BUTTON_HOME);
    GetButton<u16>(m_left_button, buttons, EmuWiimote::PAD_LEFT);
    GetButton<u16>(m_up_button, buttons, EmuWiimote::PAD_UP);
    GetButton<u16>(m_down_button, buttons, EmuWiimote::PAD_DOWN);
    GetButton<u16>(m_right_button, buttons, EmuWiimote::PAD_RIGHT);

    rpt.SetCoreData(core);
  }

  if (m_remote_orientation_box->isVisible() && rpt.HasAccel())
  {
    // FYI: Interleaved reports may behave funky as not all data is always available.

    AccelData accel;
    rpt.GetAccelData(&accel);

    GetSpinBoxU16(m_remote_orientation_x_value, accel.value.x);
    GetSpinBoxU16(m_remote_orientation_y_value, accel.value.y);
    GetSpinBoxU16(m_remote_orientation_z_value, accel.value.z);

    rpt.SetAccelData(accel);
  }

  if (m_ir_box->isVisible() && rpt.HasIR() && !m_use_controller->isChecked())
  {
    u8* const ir_data = rpt.GetIRDataPtr();

    u16 y = m_ir_y_value->value();
    std::array<u16, 4> x;
    x[0] = m_ir_x_value->value();
    x[1] = x[0] + 100;
    x[2] = x[0] - 10;
    x[3] = x[1] + 10;

    // FYI: This check is not entirely foolproof.
    // TODO: IR "full" mode not implemented.
    u8 mode = WiimoteEmu::CameraLogic::IR_MODE_BASIC;

    if (rpt.GetIRDataSize() == sizeof(WiimoteEmu::IRExtended) * 4)
      mode = WiimoteEmu::CameraLogic::IR_MODE_EXTENDED;
    else if (rpt.GetIRDataSize() == sizeof(WiimoteEmu::IRFull) * 2)
      mode = WiimoteEmu::CameraLogic::IR_MODE_FULL;

    if (mode == WiimoteEmu::CameraLogic::IR_MODE_BASIC)
    {
      memset(ir_data, 0xFF, sizeof(WiimoteEmu::IRBasic) * 2);
      std::array<WiimoteEmu::IRBasic, 2> ir_basic;
      for (int i = 0; i < 2; ++i)
      {
        if (x[i * 2] < 1024 && y < 768)
        {
          ir_basic[i].x1 = static_cast<u8>(x[i * 2]);
          ir_basic[i].x1hi = x[i * 2] >> 8;

          ir_basic[i].y1 = static_cast<u8>(y);
          ir_basic[i].y1hi = y >> 8;
        }
        if (x[i * 2 + 1] < 1024 && y < 768)
        {
          ir_basic[i].x2 = static_cast<u8>(x[i * 2 + 1]);
          ir_basic[i].x2hi = x[i * 2 + 1] >> 8;

          ir_basic[i].y2 = static_cast<u8>(y);
          ir_basic[i].y2hi = y >> 8;
        }
      }
      Common::BitCastPtr<std::array<WiimoteEmu::IRBasic, 2>>(ir_data) = ir_basic;
    }
    else
    {
      // TODO: this code doesnt work, resulting in no IR TAS inputs in e.g. wii sports menu when no
      // remote extension is used
      memset(ir_data, 0xFF, sizeof(WiimoteEmu::IRExtended) * 4);
      std::array<WiimoteEmu::IRExtended, 4> ir_extended;
      for (size_t i = 0; i < x.size(); ++i)
      {
        if (x[i] < 1024 && y < 768)
        {
          ir_extended[i].x = static_cast<u8>(x[i]);
          ir_extended[i].xhi = x[i] >> 8;

          ir_extended[i].y = static_cast<u8>(y);
          ir_extended[i].yhi = y >> 8;

          ir_extended[i].size = 10;
        }
      }
      Common::BitCastPtr<std::array<WiimoteEmu::IRExtended, 4>>(ir_data) = ir_extended;
    }
  }

  if (rpt.HasExt() && m_nunchuk_stick_box->isVisible())
  {
    using WiimoteEmu::Nunchuk;

    u8* const ext_data = rpt.GetExtDataPtr();

    Nunchuk::DataFormat nunchuk = Common::BitCastPtr<Nunchuk::DataFormat>(ext_data);

    GetSpinBoxU8(m_nunchuk_stick_x_value, nunchuk.jx);
    GetSpinBoxU8(m_nunchuk_stick_y_value, nunchuk.jy);

    auto accel = nunchuk.GetAccel().value;
    GetSpinBoxU16(m_nunchuk_orientation_x_value, accel.x);
    GetSpinBoxU16(m_nunchuk_orientation_y_value, accel.y);
    GetSpinBoxU16(m_nunchuk_orientation_z_value, accel.z);
    nunchuk.SetAccel(accel);

    u8 bt = nunchuk.GetButtons();
    GetButton<u8>(m_c_button, bt, WiimoteEmu::Nunchuk::BUTTON_C);
    GetButton<u8>(m_z_button, bt, WiimoteEmu::Nunchuk::BUTTON_Z);
    nunchuk.SetButtons(bt);

    Common::BitCastPtr<Nunchuk::DataFormat>(ext_data) = nunchuk;
    key.Encrypt(ext_data, 0, sizeof(Nunchuk::DataFormat));
  }

  if (m_classic_left_stick_box->isVisible())
  {
    using WiimoteEmu::Classic;

    u8* const ext_data = rpt.GetExtDataPtr();
    key.Decrypt(ext_data, 0, sizeof(Classic::DataFormat));

    Classic::DataFormat cc = Common::BitCastPtr<Classic::DataFormat>(ext_data);

    u16 bt = cc.GetButtons();
    GetButton<u16>(m_classic_a_button, bt, WiimoteEmu::Classic::BUTTON_A);
    GetButton<u16>(m_classic_b_button, bt, WiimoteEmu::Classic::BUTTON_B);
    GetButton<u16>(m_classic_x_button, bt, WiimoteEmu::Classic::BUTTON_X);
    GetButton<u16>(m_classic_y_button, bt, WiimoteEmu::Classic::BUTTON_Y);
    GetButton<u16>(m_classic_plus_button, bt, WiimoteEmu::Classic::BUTTON_PLUS);
    GetButton<u16>(m_classic_minus_button, bt, WiimoteEmu::Classic::BUTTON_MINUS);
    GetButton<u16>(m_classic_l_button, bt, WiimoteEmu::Classic::TRIGGER_L);
    GetButton<u16>(m_classic_r_button, bt, WiimoteEmu::Classic::TRIGGER_R);
    GetButton<u16>(m_classic_zl_button, bt, WiimoteEmu::Classic::BUTTON_ZL);
    GetButton<u16>(m_classic_zr_button, bt, WiimoteEmu::Classic::BUTTON_ZR);
    GetButton<u16>(m_classic_home_button, bt, WiimoteEmu::Classic::BUTTON_HOME);
    GetButton<u16>(m_classic_left_button, bt, WiimoteEmu::Classic::PAD_LEFT);
    GetButton<u16>(m_classic_up_button, bt, WiimoteEmu::Classic::PAD_UP);
    GetButton<u16>(m_classic_down_button, bt, WiimoteEmu::Classic::PAD_DOWN);
    GetButton<u16>(m_classic_right_button, bt, WiimoteEmu::Classic::PAD_RIGHT);
    cc.SetButtons(bt);

    auto right_stick = cc.GetRightStick().value;
    GetSpinBoxU8(m_classic_right_stick_x_value, right_stick.x);
    GetSpinBoxU8(m_classic_right_stick_y_value, right_stick.y);
    cc.SetRightStick(right_stick);

    auto left_stick = cc.GetLeftStick().value;
    GetSpinBoxU8(m_classic_left_stick_x_value, left_stick.x);
    GetSpinBoxU8(m_classic_left_stick_y_value, left_stick.y);
    cc.SetLeftStick(left_stick);

    u8 rt = cc.GetRightTrigger().value;
    GetSpinBoxU8(m_right_trigger_value, rt);
    cc.SetRightTrigger(rt);

    u8 lt = cc.GetLeftTrigger().value;
    GetSpinBoxU8(m_left_trigger_value, lt);
    cc.SetLeftTrigger(lt);

    Common::BitCastPtr<Classic::DataFormat>(ext_data) = cc;
    key.Encrypt(ext_data, 0, sizeof(Classic::DataFormat));
  }

  if (rpt.HasExt() && m_balance_board_box->isVisible())
  {
    using WiimoteEmu::BalanceBoard;

    u8* const ext_data = rpt.GetExtDataPtr();
    BalanceBoard::DataFormat bb_data = Common::BitCastPtr<BalanceBoard::DataFormat>(ext_data);

    // TODO: Reading the existing values, but then just clobbering them instead of using them if
    // controller input is enabled
    double top_right = BalanceBoard::ConvertToKilograms(Common::swap16(bb_data.top_right));
    double bottom_right = BalanceBoard::ConvertToKilograms(Common::swap16(bb_data.bottom_right));
    double top_left = BalanceBoard::ConvertToKilograms(Common::swap16(bb_data.top_left));
    double bottom_left = BalanceBoard::ConvertToKilograms(Common::swap16(bb_data.bottom_left));

    top_right = m_top_right_balance_value->value();
    bottom_right = m_bottom_right_balance_value->value();
    top_left = m_top_left_balance_value->value();
    bottom_left = m_bottom_left_balance_value->value();

    bb_data.top_right = Common::swap16(BalanceBoard::ConvertToSensorWeight(top_right));
    bb_data.bottom_right = Common::swap16(BalanceBoard::ConvertToSensorWeight(bottom_right));
    bb_data.top_left = Common::swap16(BalanceBoard::ConvertToSensorWeight(top_left));
    bb_data.bottom_left = Common::swap16(BalanceBoard::ConvertToSensorWeight(bottom_left));
    bb_data.temperature = BalanceBoard::TEMPERATURE;
    bb_data.battery = 0x83;

    Common::BitCastPtr<BalanceBoard::DataFormat>(ext_data) = bb_data;
    key.Encrypt(ext_data, 0, sizeof(BalanceBoard::DataFormat));
  }
}
