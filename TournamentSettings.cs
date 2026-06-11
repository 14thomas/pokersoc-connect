using System.Text.Json;
using System.Text.Json.Serialization;

namespace pokersoc_connect
{
  public sealed class TournamentSettings
  {
    public int ArcMemberBuyInCents { get; set; } = 1500;
    public int NonArcMemberBuyInCents { get; set; } = 2000;
    public int ArcMemberRebuyCents { get; set; } = 1500;
    public int NonArcMemberRebuyCents { get; set; } = 2000;
    public bool ArcOnly { get; set; }
    /// <summary>Max rebuys per player. 0 = unlimited.</summary>
    public int RebuyCap { get; set; }

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
      PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
      WriteIndented = true,
      DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
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

    public int GetPriceCents(bool isArc, bool isRebuy)
    {
      if (isArc)
        return isRebuy ? ArcMemberRebuyCents : ArcMemberBuyInCents;
      return isRebuy ? NonArcMemberRebuyCents : NonArcMemberBuyInCents;
    }

    public string GetBuyInLabel(bool isArc, bool isRebuy)
    {
      var tier = isArc ? "ARC Member" : "Non-ARC Member";
      return isRebuy ? $"{tier} Rebuy" : $"{tier} Buy-in";
    }
  }
}
