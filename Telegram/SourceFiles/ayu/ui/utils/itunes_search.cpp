// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/utils/itunes_search.h"

namespace Ayu::Ui::Itunes {

QPixmap FetchCover(const QString &performer,
                   const QString &title,
                   int sizeHintPx,
                   int timeoutMs) {
    // External iTunes search disabled for privacy and minimalism
    return QPixmap();
}

}
