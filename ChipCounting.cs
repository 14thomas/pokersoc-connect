namespace pokersoc_connect
{
  /// <summary>
  /// Chip stack counting: 25c and $25 chips are counted in stacks of 4; others use 5.
  /// </summary>
  public static class ChipCounting
  {
    public const int StackMultiplierMode = 5;

    public static bool IsStackOfFourDenom(int denomCents) => denomCents is 25 or 2500;

    public static int GetAddAmount(int denomCents, int multiplier)
    {
      if (multiplier == StackMultiplierMode)
        return IsStackOfFourDenom(denomCents) ? 4 : 5;
      return multiplier;
    }
  }
}
