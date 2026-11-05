#include "arbtok/tashkeel.hpp"

#include "arbtok/ensemble.hpp"
#include "arbtok/waqf.hpp"

namespace arbtok::tashkeel {

TashkeelDiacritizer::TashkeelDiacritizer() : diacritizer_(&ensemble::get_ensemble()) {}

std::string TashkeelDiacritizer::diacritize(const std::string& text, bool pausal) const {
    std::string diacritized;
    try {
        diacritized = diacritizer_->diacritize(text);
    } catch (const Error& e) {
        throw TashkeelError(e.message());
    } catch (const std::exception& e) {
        throw TashkeelError(e.what());
    }
    return pausal ? waqf::pausal(diacritized) : diacritized;
}

}  // namespace arbtok::tashkeel
