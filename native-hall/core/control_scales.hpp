#pragma once

namespace native_hall::scales {
// Original v4.4 displayed scales. These are control labels, not measured T60.
inline constexpr float times[]={0.6f,0.6f,0.8f,0.9f,1.1f,1.2f,1.3f,1.4f,1.5f,1.7f,1.8f,2,2.2f,2.4f,2.6f,2.8f,
    3,3.4f,3.8f,4.2f,4.6f,5.2f,5.7f,6.5f,7.5f,8.5f,10,12,16,22,35,70};
inline constexpr int frequencies[]={100,100,200,300,420,540,660,780,920,1000,1200,1300,1500,1700,1830,2000,
    2200,2400,2600,2800,3100,3400,3700,4000,4400,4800,5300,5900,6600,7500,8800,10900};
} // namespace native_hall::scales
