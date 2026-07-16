using System;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace pokersoc_connect
{
  public enum TournamentPricingStructure
  {
    ArcVsNonArc,
    MembershipTiers
  }

  public sealed class TournamentSettings
  {
    public TournamentPricingStructure PricingStructure { get; set; } = TournamentPricingStructure.ArcVsNonArc;

    public int ArcMemberBuyInCents { get; set; } = 1000;
    public int NonArcMemberBuyInCents { get; set; } = 1500;
    public int ArcMemberRebuyCents { get; set; } = 1000;
    public int NonArcMemberRebuyCents { get; set; } = 1500;
    public bool ArcOnly { get; set; }

    public int BronzeBuyInCents { get; set; } = 2500;
    public int SilverBuyInCents { get; set; } = 2300;
    public int GoldBuyInCents { get; set; } = 2300;
    public int PlatinumBuyInCents { get; set; } = 2300;
    public int DiamondBuyInCents { get; set; } = 2300;

    public int BronzeRebuyCents { get; set; } = 2500;
    public int SilverRebuyCents { get; set; } = 2300;
    public int GoldRebuyCents { get; set; } = 2300;
    public int PlatinumRebuyCents { get; set; } = 2300;
    public int DiamondRebuyCents { get; set; } = 2300;

    /// <summary>Max rebuys per player. 0 = unlimited.</summary>
    public int RebuyCap { get; set; }

    public static readonly string[] MembershipTiers = { "Bronze", "Silver", "Gold", "Platinum", "Diamond" };

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
      PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
      WriteIndented = true,
      DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
      Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) }
    };

    public static TournamentSettings Default() => new();

    public string ToJson() => JsonSerializer.Serialize(this, JsonOptions);

    public static TournamentSettings FromJson(string? json)
    {
      if (string.IsNullOrWhiteSpace(json)) return Default();
      try
      {
        return JsonSerializer.Deserialize<TournamentSettings>(json, JsonOptions) ?? Default();
      }
      catch
      {
        return Default();
      }
    }

    public bool CanBuyIn(int existingBuyInCount)
      => RebuyCap <= 0 || existingBuyInCount < 1 + RebuyCap;

    public bool UsesMembershipPricing
      => PricingStructure == TournamentPricingStructure.MembershipTiers;

    public int GetArcPriceCents(bool isArc, bool isRebuy)
    {
      if (isArc)
        return isRebuy ? ArcMemberRebuyCents : ArcMemberBuyInCents;
      return isRebuy ? NonArcMemberRebuyCents : NonArcMemberBuyInCents;
    }

    public string GetArcBuyInLabel(bool isArc, bool isRebuy)
    {
      var tier = isArc ? "ARC Member" : "Non-ARC Member";
      return isRebuy ? $"{tier} Rebuy" : $"{tier} Buy-in";
    }

    public int GetMembershipPriceCents(string membershipType, bool isRebuy)
    {
      return membershipType switch
      {
        "Bronze" => isRebuy ? BronzeRebuyCents : BronzeBuyInCents,
        "Silver" => isRebuy ? SilverRebuyCents : SilverBuyInCents,
        "Gold" => isRebuy ? GoldRebuyCents : GoldBuyInCents,
        "Platinum" => isRebuy ? PlatinumRebuyCents : PlatinumBuyInCents,
        "Diamond" => isRebuy ? DiamondRebuyCents : DiamondBuyInCents,
        _ => isRebuy ? BronzeRebuyCents : BronzeBuyInCents
      };
    }

    public string GetMembershipBuyInLabel(string membershipType, bool isRebuy)
      => isRebuy ? $"{membershipType} Rebuy" : $"{membershipType} Buy-in";

    public string GetPricingStructureSummary()
    {
      return PricingStructure switch
      {
        TournamentPricingStructure.MembershipTiers => "Membership tiers (Bronze–Diamond)",
        _ => ArcOnly ? "ARC vs Non-ARC (ARC only)" : "ARC vs Non-ARC"
      };
    }

    // Backwards-compatible aliases used by older call sites during migration
    public int GetPriceCents(bool isArc, bool isRebuy) => GetArcPriceCents(isArc, isRebuy);
    public string GetBuyInLabel(bool isArc, bool isRebuy) => GetArcBuyInLabel(isArc, isRebuy);
  }
}
