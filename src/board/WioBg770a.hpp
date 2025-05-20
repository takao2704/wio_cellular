/*
 * WioBg770a.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef WIOBG770A_HPP
#define WIOBG770A_HPP

#include <Arduino.h>
#include "Suli3.hpp"

namespace wiocellular
{
    namespace board
    {

        /**
         * @~Japanese
         * @brief Seeed Studio Wio BG770Aボード
         *
         * @tparam MODULE モジュールのクラス
         * @tparam INTERFACE インターフェースのクラス
         *
         * Seeed Studio Wio BG770Aボードのクラスです。
         */
        template <typename MODULE, typename INTERFACE>
        class WioBg770a : public MODULE
        {
        private:
            suli3::arduino::DigitalOutputPin<PIN_VGROVE_ENABLE> VgroveEnable_;

        public:
            /**
             * @~Japanese
             * @brief コンストラクタ
             *
             * @param [in] interface インターフェースのインスタンス
             *
             * コンストラクタ。
             * interfaceにインターフェースのインスタンスを指定します。
             */
            explicit WioBg770a(INTERFACE &interface)
                : MODULE{interface}
            {
            }

            /**
             * @~Japanese
             * @brief ボードを開始
             *
             * ボードを初期化します。
             */
            void begin(void)
            {
                MODULE::getInterface().begin();

                VgroveEnable_.begin(OUTPUT, 1);
            }

            /**
             * @~Japanese
             * @brief [DEPRECATED]セルラーモジュールの電源を投入
             *
             * セルラーモジュールの電源を投入します。
             */
            [[deprecated]]
            void enableCellularPower(void)
            {
            }

            /**
             * @~Japanese
             * @brief [DEPRECATED]セルラーモジュールの電源を開放
             *
             * セルラーモジュールの電源を開放します。
             */
            [[deprecated]]
            void disableCellularPower(void)
            {
            }

            /**
             * @~Japanese
             * @brief [DEPRECATED]Groveの電源を投入
             *
             * Groveの電源を投入します。
             */
            [[deprecated("Use digitalWrite(PIN_VGROVE_ENABLE, VGROVE_ENABLE_ON) instead.")]]
            void enableGrovePower(void)
            {
                VgroveEnable_.write(0);
                delay(2 + 2);
            }

            /**
             * @~Japanese
             * @brief [DEPRECATED]Groveの電源を開放
             *
             * Groveの電源を開放します。
             */
            [[deprecated("Use digitalWrite(PIN_VGROVE_ENABLE, VGROVE_ENABLE_OFF) instead.")]]
            void disableGrovePower(void)
            {
                VgroveEnable_.write(1);
                delay(2 + 2);
            }
        };

    }
}

#endif // WIOBG770A_HPP
