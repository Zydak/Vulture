void TintColors(Material mat, float eta, out float F0, out vec3 Csheen, out vec3 Cspec0)
{
	float lum = CalculateLuminance(mat.Albedo.xyz);
	vec3 ctint = lum > 0.0 ? mat.Albedo.xyz / lum : vec3(1.0);

	F0 = (1.0 - eta) / (1.0 + eta);
	F0 *= F0;

	Cspec0 = F0 * mix(vec3(1.0), ctint, mat.SpecularTint);
	Csheen = mix(vec3(1.0), ctint, mat.SheenTint);
}

float SchlickWeight(float u)
{
	float m = clamp(1.0 - u, 0.0, 1.0);
	float m2 = m * m;
	return m2 * m2 * m;
}

vec3 EvaluateDisneyDiffuse(Material mat, vec3 Csheen, vec3 V, vec3 L, vec3 H, out float pdf)
{
	pdf = 0.0;
	if (L.z <= 0.0)
		return vec3(0.0);

	float LDotH = dot(L, H);

	float Rr = 2.0 * mat.Roughness * LDotH * LDotH;

	// Diffuse
	float FL = SchlickWeight(L.z);
	float FV = SchlickWeight(V.z);
	float Fretro = Rr * (FL + FV + FL * FV * (Rr - 1.0));
	float Fd = (1.0 - 0.5 * FL) * (1.0 - 0.5 * FV);

	// Dubsurface
	float Fss90 = 0.5 * Rr;
	float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
	float ss = 1.25 * (Fss * (1.0 / (L.z + V.z) - 0.5) + 0.5);

	// Sheen
	float FH = SchlickWeight(LDotH);
	vec3 Fsheen = FH * mat.Sheen * Csheen;

	pdf = L.z * M_1_OVER_PI;
	return M_1_OVER_PI * mat.Albedo.xyz * mix(Fd + Fretro, ss, mat.Subsurface) + Fsheen;
}

float GTR2Anisotropic(float NDotH, float HDotX, float HDotY, float ax, float ay)
{
	float a = HDotX / ax;
	float b = HDotY / ay;
	float c = a * a + b * b + NDotH * NDotH;
	return 1.0 / (M_PI * ax * ay * c * c);
}

float SmithGAnisotropic(float NDotV, float VDotX, float VDotY, float ax, float ay)
{
	float a = VDotX * ax;
	float b = VDotY * ay;
	float c = NDotV;
	return (2.0 * NDotV) / (NDotV + sqrt(a * a + b * b + c * c));
}

vec3 EvaluateMicrofacetReflection(Material mat, vec3 V, vec3 L, vec3 H, vec3 F, out float pdf)
{
	pdf = 0.0;
	if (L.z <= 0.0)
		return vec3(0.0);

	float D = GTR2Anisotropic(H.z, H.x, H.y, mat.ax, mat.ay);
	float G1 = SmithGAnisotropic(abs(V.z), V.x, V.y, mat.ax, mat.ay);
	float G2 = G1 * SmithGAnisotropic(abs(L.z), L.x, L.y, mat.ax, mat.ay);

	pdf = G1 * D / (4.0 * V.z);
	return F * D * G2 / (4.0 * L.z * V.z);
}

vec3 EvalMicrofacetRefraction(Material mat, float eta, vec3 V, vec3 L, vec3 H, vec3 F, out float pdf)
{
	pdf = 0.0;
	if (L.z >= 0.0)
		return vec3(0.0);

	float LDotH = dot(L, H);
	float VDotH = dot(V, H);

	float D = GTR2Anisotropic(H.z, H.x, H.y, mat.ax, mat.ay);
	float G1 = SmithGAnisotropic(abs(V.z), V.x, V.y, mat.ax, mat.ay);
	float G2 = G1 * SmithGAnisotropic(abs(L.z), L.x, L.y, mat.ax, mat.ay);
	float denom = LDotH + VDotH * eta;
	denom *= denom;
	float eta2 = eta * eta;
	float jacobian = abs(LDotH) / denom;

	pdf = G1 * max(0.0, VDotH) * D * jacobian / V.z;
	return pow(mat.Albedo.xyz, vec3(0.5)) * (1.0 - F) * D * G2 * abs(VDotH) * jacobian * eta2 / abs(L.z * V.z);
}

float GTR1(float NDotH, float a)
{
	if (a >= 1.0)
		return M_1_OVER_PI;
	float a2 = a * a;
	float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
	return (a2 - 1.0) / (M_PI * log(a2) * t);
}

float SmithG(float NDotV, float alphaG)
{
	float a = alphaG * alphaG;
	float b = NDotV * NDotV;
	return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

vec3 EvalClearcoat(Material mat, vec3 V, vec3 L, vec3 H, out float pdf)
{
	pdf = 0.0;
	if (L.z <= 0.0)
		return vec3(0.0);

	float VDotH = dot(V, H);

	float F = mix(0.04, 1.0, SchlickWeight(VDotH));
	float D = GTR1(H.z, mat.ClearcoatRoughness);
	float G = SmithG(L.z, 0.25) * SmithG(V.z, 0.25);
	float jacobian = 1.0 / (4.0 * VDotH);

	pdf = D * H.z * jacobian;
	return vec3(F) * D * G;
}

vec3 CosineSampleHemisphere(float r1, float r2)
{
	vec3 dir;
	float r = sqrt(r1);
	float phi = M_TWO_PI * r2;
	dir.x = r * cos(phi);
	dir.y = r * sin(phi);
	dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));
	return dir;
}

vec3 SampleGGXVNDF(vec3 V, float ax, float ay, float r1, float r2)
{
	vec3 Vh = normalize(vec3(ax * V.x, ay * V.y, V.z));

	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
	vec3 T2 = cross(Vh, T1);

	float r = sqrt(r1);
	float phi = 2.0 * M_PI * r2;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

	return normalize(vec3(ax * Nh.x, ay * Nh.y, max(0.0, Nh.z)));
}

float DielectricFresnel(float cosThetaI, float eta)
{
	float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

	if (sinThetaTSq > 1.0)
		return 1.0;

	float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

	float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
	float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

	return 0.5f * (rs * rs + rp * rp);
}

vec3 EvaluateDisney(Material mat, vec3 V, Surface surface, vec3 L, out float pdf)
{
	pdf = 0.0;
	vec3 f = vec3(0.0);

	const vec3 tangent = surface.Tangent;
	const vec3 bitangent = surface.Bitangent;

	// Transform to tangent space to simplify calculations
	V = WorldToTangent(tangent, bitangent, surface.GeoNormal, V);
	L = WorldToTangent(tangent, bitangent, surface.GeoNormal, L);

	vec3 H;
	if (L.z > 0.0)
		H = normalize(L + V);
	else
		H = normalize(L + V * mat.eta);

	if (H.z < 0.0)
		H = -H;

	// Tint colors
	vec3 Csheen, Cspec0;
	float F0;
	TintColors(mat, mat.eta, F0, Csheen, Cspec0);

	// Model weights
	float dielectricWeight = (1.0 - mat.Metallic) * (1.0 - mat.SpecTrans);
	float metalWeight = mat.Metallic;
	float glassWeight = (1.0 - mat.Metallic) * mat.SpecTrans;

	// Lobe probabilities
	float schlickWeight = SchlickWeight(V.z);

	float diffProbability = dielectricWeight * CalculateLuminance(mat.Albedo.xyz);
	float dielectricProbability = dielectricWeight * CalculateLuminance(mix(Cspec0, vec3(1.0), schlickWeight));
	float metalProbability = metalWeight * CalculateLuminance(mix(mat.Albedo.xyz, vec3(1.0), schlickWeight));
	float glassProbability = glassWeight;
	float clearCoatProbability = 0.25 * mat.Clearcoat;

	// Normalize probabilities
	float TotalWeight = 1.0 / (diffProbability + dielectricProbability + metalProbability + glassProbability + clearCoatProbability);
	diffProbability *= TotalWeight;
	dielectricProbability *= TotalWeight;
	metalProbability *= TotalWeight;
	glassProbability *= TotalWeight;
	clearCoatProbability *= TotalWeight;

	bool reflect = L.z * V.z > 0;

	float tmpPdf = 0.0;
	float VDotH = abs(dot(V, H));

	// Diffuse
	if (diffProbability > 0.0 && reflect)
	{
		f += EvaluateDisneyDiffuse(mat, Csheen, V, L, H, tmpPdf) * dielectricWeight;
		pdf += tmpPdf * diffProbability;
	}

	// Dielectric Reflection
	if (dielectricProbability > 0.0 && reflect)
	{
		// Normalize for interpolating based on Cspec0
		float F = (DielectricFresnel(VDotH, 1.0 / mat.Ior) - F0) / (1.0 - F0);

		f += EvaluateMicrofacetReflection(mat, V, L, H, mix(Cspec0, vec3(1.0), F), tmpPdf) * dielectricWeight;
		pdf += tmpPdf * dielectricProbability;
	}

	// Metallic Reflection
	if (metalProbability > 0.0 && reflect)
	{
		// Tinted to base color
		vec3 F = mix(mat.Albedo.xyz, vec3(1.0), SchlickWeight(VDotH));

		f += EvaluateMicrofacetReflection(mat, V, L, H, F, tmpPdf) * metalWeight;
		pdf += tmpPdf * metalProbability;
	}

	// Glass/Specular BSDF
	if (glassProbability > 0.0)
	{
		// Dielectric fresnel (achromatic)
		float F = DielectricFresnel(VDotH, mat.eta);

		if (reflect)
		{
			f += EvaluateMicrofacetReflection(mat, V, L, H, vec3(F), tmpPdf) * glassWeight;
			pdf += tmpPdf * glassProbability * F;
		}
		else
		{
			f += EvalMicrofacetRefraction(mat, mat.eta, V, L, H, vec3(F), tmpPdf) * glassWeight;
			pdf += tmpPdf * glassProbability * (1.0 - F);
		}
	}

	// Clearcoat
	if (clearCoatProbability > 0.0 && reflect)
	{
		f += EvalClearcoat(mat, V, L, H, tmpPdf) * 0.25 * mat.Clearcoat;
		pdf += tmpPdf * clearCoatProbability;
	}
	return f * abs(L.z);
}

vec3 SampleGTR1(float rgh, float r1, float r2)
{
	float a = max(0.001, rgh);
	float a2 = a * a;

	float phi = r1 * M_TWO_PI;

	float cosTheta = sqrt((1.0 - pow(a2, 1.0 - r2)) / (1.0 - a2));
	float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
	float sinPhi = sin(phi);
	float cosPhi = cos(phi);

	return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 SampleDisney(Material mat, vec3 V, Surface surface, out vec3 L, out float pdf)
{
	pdf = 0.0;

	const vec3 tangent = surface.Tangent;
	const vec3 bitangent = surface.Bitangent;

	float r1 = Rnd(payload.Seed);
	float r2 = Rnd(payload.Seed);

	// Transform to tangent space to simplify calculations
	V = WorldToTangent(tangent, bitangent, surface.Normal, V);

	// Tint colors
	vec3 Csheen, Cspec0;
	float F0;
	TintColors(mat, mat.eta, F0, Csheen, Cspec0);

	// Model weights
	float dielectricWeight = (1.0 - mat.Metallic) * (1.0 - mat.SpecTrans);
	float metalWeight = mat.Metallic;
	float glassWeight = (1.0 - mat.Metallic) * mat.SpecTrans;

	// Lobe probabilities
	float schlickWeight = SchlickWeight(V.z);

	float diffProbability = dielectricWeight * CalculateLuminance(mat.Albedo.xyz);
	float dielectricProbability = dielectricWeight * CalculateLuminance(mix(Cspec0, vec3(1.0), schlickWeight));
	float metalProbability = metalWeight * CalculateLuminance(mix(mat.Albedo.xyz, vec3(1.0), schlickWeight));
	float glassProbability = glassWeight;
	float clearCoatProbability = 0.25 * mat.Clearcoat;

	// Normalize probabilities
	float TotalWeight = (diffProbability + dielectricProbability + metalProbability + glassProbability + clearCoatProbability);
	diffProbability /= TotalWeight;
	dielectricProbability /= TotalWeight;
	metalProbability /= TotalWeight;
	glassProbability /= TotalWeight;
	clearCoatProbability /= TotalWeight;

	// CDF of the sampling probabilities
	float diffuseCDF = diffProbability;
	float dielectricCDF = diffuseCDF + dielectricProbability;
	float metallicCDF = dielectricCDF + metalProbability;
	float glassCDF = metallicCDF + glassProbability;
	float clearCoatCDF= glassCDF + clearCoatProbability;

	// Sample a lobe based on its importance
	float r3 = Rnd(payload.Seed);

	if (r3 < diffuseCDF) // Diffuse
	{
		L = CosineSampleHemisphere(r1, r2);
	}
	else if (r3 < metallicCDF) // Dielectric + Metallic reflection
	{
		vec3 H = SampleGGXVNDF(V, mat.ax, mat.ay, r1, r2);

		if (H.z < 0.0)
			H = -H;

		L = normalize(reflect(-V, H));
	}
	else if (r3 < glassCDF) // Glass
	{
		vec3 H = SampleGGXVNDF(V, mat.ax, mat.ay, r1, r2);
		float F = DielectricFresnel(abs(dot(V, H)), mat.eta);

		if (H.z < 0.0)
			H = -H;

		// Rescale random number for reuse
		r3 = (r3 - metallicCDF) / (glassCDF - metallicCDF);

		// Reflection
		if (r3 < F)
		{
			L = normalize(reflect(-V, H));
		}
		else // Transmission
		{
			L = normalize(refract(-V, H, mat.eta));
		}
	}
	else // Clearcoat
	{
		vec3 H = SampleGTR1(mat.ClearcoatRoughness, r1, r2);

		if (H.z < 0.0)
			H = -H;

		L = normalize(reflect(-V, H));
	}

	L = TangentToWorld(tangent, bitangent, surface.Normal, L);
	V = TangentToWorld(tangent, bitangent, surface.Normal, V);

	return EvaluateDisney(mat, V, surface, L, pdf);
}