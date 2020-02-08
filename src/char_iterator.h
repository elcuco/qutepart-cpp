#pragma once

#include "text_pos.h"

namespace Qutepart {

class CharIterator {
public:
    // create iterator and make first step
    CharIterator(const TextPosition& position);

    QChar step();  // return current character and then make step back
    TextPosition currentPosition() const;
    bool atEnd() const;

protected:
    TextPosition position_;

    virtual void movePosition() = 0;
};

class ForwardCharIterator: public CharIterator {
    using CharIterator::CharIterator;
private:
    virtual void movePosition() override;
};

class BackwardCharIterator: public CharIterator {
    using CharIterator::CharIterator;
private:
    virtual void movePosition() override;
};

}  // namespace Qutepart
