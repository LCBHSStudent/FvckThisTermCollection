#include "PokemonSkill.h"
#include "PokemonBase.h"

#define REGISTER_SKILL REGISTER_METHOD

#define JUDGE_ATTR(_ATTR) \
    (dest->m_pkmAttr == PokemonBase::PokemonAttribute::_ATTR)

#define ATK_DEBUG(_SKILL) \
    std::cout << user->m_name \
              << "ʹ��" #_SKILL "������" \
              << dest->m_name << "\n"

#define RAND_OZ \
    PokemonSkill::s_distr(PokemonSkill::s_engine)

QHash<QString, PokemonSkill::SkillFunc> 
    PokemonSkill::s_skillMap = {};

std::random_device
    PokemonSkill::s_rdev;
std::default_random_engine 
    PokemonSkill::s_engine(s_rdev());
std::uniform_real_distribution<float> 
    PokemonSkill::s_distr(0.0f, 1.0f);

REGISTER_SKILL(JJJJ) {
    ATK_DEBUG(��ͨ����);
}

REGISTER_SKILL(FireBall) {
    ATK_DEBUG(����);
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
    
}

REGISTER_SKILL(GreassLeaf) {
    ATK_DEBUG(Ҷ��);
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
}

REGISTER_SKILL(WaterBullet) {
    ATK_DEBUG(ˮ��);
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
}

REGISTER_SKILL(WindBreath) {
    ATK_DEBUG(��Ϣ);
    //---�����˺�---//
    
    if JUDGE_ATTR(GRASS) {
        
    } else if JUDGE_ATTR(WATER) {
        
    } else {
        
    }
    
    //------����buff-----//
    //--------Ч��-------//
    //return x (buff id)//
}
